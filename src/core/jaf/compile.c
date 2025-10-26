/* Copyright (C) 2019 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/buffer.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/jaf.h"

/*
 * NOTE: We need to pass some state between calls to handle
 *       break/continue.
 */
struct loop_state {
	size_t loop_addr;
	size_t nr_breaks;
	uint32_t *breaks;
};

struct label {
	const char *name;
	size_t addr;
	struct jaf_block_item *stmt;
};

struct scope {
	int nr_vars;
	int *vars;
};

struct compiler_state {
	struct ain *ain;
	struct buffer out;
	int func_no;
	int super_no;
	size_t nr_loops;
	struct loop_state *loops;
	size_t nr_labels;
	struct label *labels;
	size_t nr_gotos;
	struct label *gotos;
	size_t nr_scopes;
	struct scope *scopes;
};

static int get_string_no(struct compiler_state *state, const char *s)
{
	char *u = conv_output(s);
	int i = ain_add_string(state->ain, u);
	free(u);
	return i;
}

static void write_opcode(struct compiler_state *state, uint16_t opcode)
{
	buffer_write_int16(&state->out, opcode);
}

static void write_argument(struct compiler_state *state, uint32_t arg)
{
	buffer_write_int32(&state->out, arg);
}

static void write_instruction0(struct compiler_state *state, uint16_t opcode)
{
	if (AIN_VERSION_GTE(state->ain, 14, 0)) {
		if (opcode == REF) {
			write_opcode(state, X_REF);
			write_argument(state, 1);
		} else if (opcode == REFREF) {
			write_opcode(state, X_REF);
			write_argument(state, 2);
		} else if (opcode == DUP) {
			write_opcode(state, X_DUP);
			write_argument(state, 1);
		} else if (opcode == DUP2) {
			write_opcode(state, X_DUP);
			write_argument(state, 2);
		} else if (opcode == ASSIGN) {
			write_opcode(state, X_ASSIGN);
			write_argument(state, 1);
		} else {
			write_opcode(state, opcode);
		}
	} else {
		write_opcode(state, opcode);
	}
}

static void write_instruction1(struct compiler_state *state, uint16_t opcode, uint32_t arg0)
{
	if (opcode == S_MOD && state->ain->version <= 8) {
		write_instruction1(state, PUSH, arg0);
		write_instruction0(state, S_MOD);
	} else if (opcode == SH_LOCALDELETE && AIN_VERSION_GTE(state->ain, 12, 0)) {
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, arg0);
		write_instruction0(state, DUP2);
		write_instruction0(state, REF);
		write_instruction0(state, DELETE);
		write_instruction1(state, PUSH, -1);
		write_instruction0(state, ASSIGN);
		write_instruction0(state, POP);
	} else if (opcode == SH_LOCALREF && AIN_VERSION_GTE(state->ain, 6, 1)) {
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, arg0);
		write_instruction0(state, REF);
	} else if (opcode == SH_LOCALINC && AIN_VERSION_GTE(state->ain, 6, 1)) {
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, arg0);
		write_instruction0(state, INC);
	} else if (opcode == SH_LOCALDEC && AIN_VERSION_GTE(state->ain, 6, 1)) {
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, arg0);
		write_instruction0(state, DEC);
	} else {
		write_opcode(state, opcode);
		write_argument(state, arg0);
	}
}

static void write_instruction2(struct compiler_state *state, uint16_t opcode, uint32_t arg0, uint32_t arg1)
{
	if (opcode == SH_LOCALCREATE && state->ain->version >= 12) {
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, arg0);
		write_instruction0(state, DUP2);
		write_instruction0(state, REF);
		write_instruction0(state, DELETE);
		write_instruction0(state, DUP2);
		write_instruction2(state, NEW, arg1, -1);
		write_instruction0(state, ASSIGN);
		write_instruction0(state, POP);
		write_instruction0(state, POP);
		write_instruction0(state, POP);
	} else if (opcode == SH_LOCALASSIGN && AIN_VERSION_GTE(state->ain, 6, 1)) {
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, arg0);
		write_instruction1(state, PUSH, arg1);
		write_instruction0(state, ASSIGN);
		write_instruction0(state, POP);
	} else {
		write_opcode(state, opcode);
		write_argument(state, arg0);
		write_argument(state, arg1);
	}
}

static void write_instruction3(struct compiler_state *state, uint16_t opcode, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	write_opcode(state, opcode);
	write_argument(state, arg0);
	write_argument(state, arg1);
	write_argument(state, arg2);
}

static void write_CALLHLL(struct compiler_state *state, const char *lib, const char *fun, int type)
{
	int libno = ain_get_library(state->ain, lib);
	write_instruction3(state, CALLHLL, libno, ain_get_library_function(state->ain, libno, fun), type);
}

static void start_loop(struct compiler_state *state)
{
	state->loops = xrealloc_array(state->loops, state->nr_loops, state->nr_loops+1, sizeof(struct loop_state));
	state->loops[state->nr_loops].loop_addr = state->out.index;
	state->loops[state->nr_loops].nr_breaks = 0;
	state->loops[state->nr_loops].breaks = NULL;
	state->nr_loops++;
}

static void end_loop(struct compiler_state *state)
{
	struct loop_state *loop = &state->loops[--state->nr_loops];
	for (size_t i = 0; i < loop->nr_breaks; i++) {
		buffer_write_int32_at(&state->out, loop->breaks[i], state->out.index);
	}
	free(loop->breaks);
}

static void start_scope(struct compiler_state *state)
{
	state->scopes = xrealloc_array(state->scopes, state->nr_scopes, state->nr_scopes+1, sizeof(struct scope));
	state->scopes[state->nr_scopes].vars = NULL;
	state->scopes[state->nr_scopes].nr_vars = 0;
	state->nr_scopes++;
}

static void compile_local_ref(struct compiler_state *state, int var_no);
static void compile_delete_var(struct compiler_state *state, int var_no)
{
	struct ain_function *f = &state->ain->functions[state->func_no];
	struct ain_variable *v = &f->vars[var_no];
	switch (v->type.data) {
	case AIN_STRUCT:
	case AIN_ARRAY:
	case AIN_REF_TYPE:
	case AIN_IFACE:
		write_instruction1(state, SH_LOCALDELETE, var_no);
		break;
	case AIN_ARRAY_TYPE:
		compile_local_ref(state, var_no);
		write_instruction0(state, A_FREE);
		break;
	default:
		break;
	}
}

static void end_scope(struct compiler_state *state)
{
	struct scope *scope = &state->scopes[--state->nr_scopes];

	// XXX: no need to delete at end of function scope
	if (state->nr_scopes == 0) {
		free(scope->vars);
		return;
	}

	for (int i = scope->nr_vars - 1; i >= 0; i--) {
		compile_delete_var(state, scope->vars[i]);
	}
	free(scope->vars);
}

static void scope_add_variable(struct compiler_state *state, int var_no)
{
	struct scope *scope = &state->scopes[state->nr_scopes-1];
	scope->vars = xrealloc_array(scope->vars, scope->nr_vars, scope->nr_vars+1, sizeof(int));
	scope->vars[scope->nr_vars++] = var_no;
}

static void start_function(struct compiler_state *state, int func_no, int super_no)
{
	state->nr_labels = 0;
	state->nr_gotos = 0;
	state->labels = NULL;
	state->gotos = NULL;
	state->func_no = func_no;
	state->super_no = super_no;
	start_scope(state);
}

static void end_function(struct compiler_state *state)
{
	for (size_t i = 0; i < state->nr_gotos; i++) {
		// find corresponding label
		struct label *label = NULL;
		for (size_t j = 0; j < state->nr_labels; j++) {
			if (!strcmp(state->labels[j].name, state->gotos[i].name)) {
				label = &state->labels[j];
				break;
			}
		}
		if (!label) {
			JAF_ERROR(state->gotos[i].stmt, "Undefined label");
		}

		// write label address into JUMP argument
		buffer_write_int32_at(&state->out, state->gotos[i].addr, label->addr);
	}

	end_scope(state);
	free(state->labels);
	free(state->gotos);
}

#define AIN_REF_SCALAR_TYPE \
	AIN_REF_INT: \
	case AIN_REF_BOOL: \
	case AIN_REF_FLOAT: \
	case AIN_REF_LONG_INT: \
	case AIN_REF_FUNC_TYPE

static bool is_ref_scalar(enum ain_data_type type)
{
	switch (type) {
	case AIN_REF_SCALAR_TYPE:
		return true;
	default:
		return false;
	}
}

static uint32_t flo2int(float f)
{
	union { uint32_t i; float f; } v = { .f = f};
	return v.i;
}

static int incdec_instruction(enum jaf_operator op, enum ain_data_type t)
{
	if (t == AIN_LONG_INT || t == AIN_REF_LONG_INT) {
		switch (op) {
		case JAF_PRE_INC:
		case JAF_POST_INC:
			return LI_INC;
		case JAF_PRE_DEC:
		case JAF_POST_DEC:
			return LI_DEC;
		default:
			_COMPILER_ERROR(NULL, -1, "Invalid inc/dec expression");
		}
	}
	switch (op) {
	case JAF_PRE_INC:
	case JAF_POST_INC:
		return INC;
	case JAF_PRE_DEC:
	case JAF_POST_DEC:
		return DEC;
	default:
		_COMPILER_ERROR(NULL, -1, "Invalid inc/dec expression");
	}
}

static enum ain_data_type strip_ref(struct ain_type *type)
{
	switch (type->data) {
	case AIN_REF_INT:             return AIN_INT;
	case AIN_REF_FLOAT:           return AIN_FLOAT;
	case AIN_REF_STRING:          return AIN_STRING;
	case AIN_REF_STRUCT:          return AIN_STRUCT;
	case AIN_REF_ENUM:            return AIN_ENUM;
	case AIN_REF_ARRAY_INT:       return AIN_ARRAY_INT;
	case AIN_REF_ARRAY_FLOAT:     return AIN_ARRAY_FLOAT;
	case AIN_REF_ARRAY_STRING:    return AIN_ARRAY_STRING;
	case AIN_REF_ARRAY_STRUCT:    return AIN_ARRAY_STRUCT;
	case AIN_REF_FUNC_TYPE:       return AIN_FUNC_TYPE;
	case AIN_REF_ARRAY_FUNC_TYPE: return AIN_ARRAY_FUNC_TYPE;
	case AIN_REF_BOOL:            return AIN_BOOL;
	case AIN_REF_ARRAY_BOOL:      return AIN_ARRAY_BOOL;
	case AIN_REF_LONG_INT:        return AIN_LONG_INT;
	case AIN_REF_ARRAY_LONG_INT:  return AIN_ARRAY_LONG_INT;
	case AIN_REF_ARRAY:           return AIN_ARRAY;
	case AIN_WRAP:
		assert(type->array_type);
		return type->array_type->data;
	default:                      return type->data;
	}
}

static bool is_numeric_ref(struct ain_type *t)
{
	switch (t->data) {
	case AIN_REF_INT:
	case AIN_REF_LONG_INT:
	case AIN_REF_BOOL:
		return true;
	default:
		return false;
	}
}

static void write_instruction_for_arithmetic_op(struct compiler_state *state,
		enum jaf_operator op, struct ain_type *lhs_type, struct ain_type *rhs_type)
{
	if (ain_is_ref_data_type(lhs_type->data)) {
		if (op == JAF_REQ) {
			write_instruction0(state, is_numeric_ref(lhs_type) ? R_EQUALE : EQUALE);
			return;
		} else if (op == JAF_RNE) {
			write_instruction0(state, is_numeric_ref(lhs_type) ? R_NOTE : NOTE);
			return;
		}
	}
#define T(t, op) (((t) << 8) | (op))
	switch (T(strip_ref(lhs_type), op)) {
	case T(AIN_INT, JAF_EQ):
	case T(AIN_LONG_INT, JAF_EQ):
	case T(AIN_BOOL, JAF_EQ):
	case T(AIN_FUNC_TYPE, JAF_EQ):      write_instruction0(state, EQUALE); break;
	case T(AIN_INT, JAF_NEQ):
	case T(AIN_LONG_INT, JAF_NEQ):
	case T(AIN_BOOL, JAF_NEQ):
	case T(AIN_FUNC_TYPE, JAF_NEQ):      write_instruction0(state, NOTE); break;
	case T(AIN_INT, JAF_PLUS):           write_instruction0(state, ADD); break;
	case T(AIN_INT, JAF_MINUS):          write_instruction0(state, SUB); break;
	case T(AIN_INT, JAF_MULTIPLY):       write_instruction0(state, MUL); break;
	case T(AIN_INT, JAF_DIVIDE):         write_instruction0(state, DIV); break;
	case T(AIN_INT, JAF_REMAINDER):      write_instruction0(state, MOD); break;
	case T(AIN_INT, JAF_LT):
case T(AIN_LONG_INT, JAF_LT):        write_instruction0(state, LT); break;
	case T(AIN_INT, JAF_GT):
	case T(AIN_LONG_INT, JAF_GT):        write_instruction0(state, GT); break;
	case T(AIN_INT, JAF_LTE):
	case T(AIN_LONG_INT, JAF_LTE):       write_instruction0(state, LTE); break;
	case T(AIN_INT, JAF_GTE):
	case T(AIN_LONG_INT, JAF_GTE):       write_instruction0(state, GTE); break;
	case T(AIN_INT, JAF_BIT_IOR):
	case T(AIN_BOOL, JAF_BIT_IOR):       write_instruction0(state, OR); break;
	case T(AIN_INT, JAF_BIT_XOR):
	case T(AIN_BOOL, JAF_BIT_XOR):       write_instruction0(state, XOR); break;
	case T(AIN_INT, JAF_BIT_AND):
	case T(AIN_BOOL, JAF_BIT_AND):       write_instruction0(state, AND); break;
	case T(AIN_INT, JAF_LSHIFT):
	case T(AIN_BOOL, JAF_LSHIFT):        write_instruction0(state, LSHIFT); break;
	case T(AIN_INT, JAF_RSHIFT):
	case T(AIN_BOOL, JAF_RSHIFT):        write_instruction0(state, RSHIFT); break;
	case T(AIN_LONG_INT, JAF_PLUS):      write_instruction0(state, LI_ADD); break;
	case T(AIN_LONG_INT, JAF_MINUS):     write_instruction0(state, LI_SUB); break;
	case T(AIN_LONG_INT, JAF_MULTIPLY):  write_instruction0(state, LI_MUL); break;
	case T(AIN_LONG_INT, JAF_DIVIDE):    write_instruction0(state, LI_DIV); break;
	case T(AIN_LONG_INT, JAF_REMAINDER): write_instruction0(state, LI_MOD); break;
	case T(AIN_FLOAT, JAF_PLUS):         write_instruction0(state, F_ADD); break;
	case T(AIN_FLOAT, JAF_MINUS):        write_instruction0(state, F_SUB); break;
	case T(AIN_FLOAT, JAF_MULTIPLY):     write_instruction0(state, F_MUL); break;
	case T(AIN_FLOAT, JAF_DIVIDE):       write_instruction0(state, F_DIV); break;
	case T(AIN_FLOAT, JAF_EQ):           write_instruction0(state, F_EQUALE); break;
	case T(AIN_FLOAT, JAF_NEQ):          write_instruction0(state, F_NOTE); break;
	case T(AIN_FLOAT, JAF_LT):           write_instruction0(state, F_LT); break;
	case T(AIN_FLOAT, JAF_GT):           write_instruction0(state, F_GT); break;
	case T(AIN_FLOAT, JAF_LTE):          write_instruction0(state, F_LTE); break;
	case T(AIN_FLOAT, JAF_GTE):          write_instruction0(state, F_GTE); break;
	case T(AIN_STRING, JAF_PLUS):        write_instruction0(state, S_ADD); break;
	case T(AIN_STRING, JAF_EQ):          write_instruction0(state, S_EQUALE); break;
	case T(AIN_STRING, JAF_NEQ):         write_instruction0(state, S_NOTE); break;
	case T(AIN_STRING, JAF_LT):          write_instruction0(state, S_LT); break;
	case T(AIN_STRING, JAF_GT):          write_instruction0(state, S_GT); break;
	case T(AIN_STRING, JAF_LTE):         write_instruction0(state, S_LTE); break;
	case T(AIN_STRING, JAF_GTE):         write_instruction0(state, S_GTE); break;
	case T(AIN_STRING, JAF_REMAINDER):
		switch (strip_ref(rhs_type)) {
		case AIN_INT:      write_instruction1(state, S_MOD, 2); break;
		case AIN_FLOAT:    write_instruction1(state, S_MOD, 3); break;
		case AIN_STRING:   write_instruction1(state, S_MOD, 4); break;
		case AIN_BOOL:     write_instruction1(state, S_MOD, 48); break;
		case AIN_LONG_INT: write_instruction1(state, S_MOD, 56); break;
		default:           _COMPILER_ERROR(NULL, -1, "Invalid type for string formatting");
		}
		break;
	default: _COMPILER_ERROR(NULL, -1, "Invalid operator: %d", op);
	}
#undef T
}

static void write_instruction_for_assignment_op(struct compiler_state *state,
		enum jaf_operator op, struct ain_type *lhs_type, struct ain_type *rhs_type)
{
#define T(op, t) (((op) << 8) | (t))
	switch (T(op, strip_ref(rhs_type))) {
	case T(JAF_ASSIGN, AIN_INT):
	case T(JAF_ASSIGN, AIN_BOOL):
	case T(JAF_ASSIGN, AIN_FUNCTION):
	case T(JAF_ASSIGN, AIN_FUNC_TYPE):       write_instruction0(state, ASSIGN); break;
	case T(JAF_ADD_ASSIGN, AIN_INT):
	case T(JAF_ADD_ASSIGN, AIN_BOOL):        write_instruction0(state, PLUSA); break;
	case T(JAF_SUB_ASSIGN, AIN_INT):
	case T(JAF_SUB_ASSIGN, AIN_BOOL):        write_instruction0(state, MINUSA); break;
	case T(JAF_MUL_ASSIGN, AIN_INT):
	case T(JAF_MUL_ASSIGN, AIN_BOOL):        write_instruction0(state, MULA); break;
	case T(JAF_DIV_ASSIGN, AIN_INT):
	case T(JAF_DIV_ASSIGN, AIN_BOOL):        write_instruction0(state, DIVA); break;
	case T(JAF_MOD_ASSIGN, AIN_INT):
	case T(JAF_MOD_ASSIGN, AIN_BOOL):        write_instruction0(state, MODA); break;
	case T(JAF_OR_ASSIGN, AIN_INT):
	case T(JAF_OR_ASSIGN, AIN_BOOL):         write_instruction0(state, ORA); break;
	case T(JAF_XOR_ASSIGN, AIN_INT):
	case T(JAF_XOR_ASSIGN, AIN_BOOL):        write_instruction0(state, XORA); break;
	case T(JAF_AND_ASSIGN, AIN_INT):
	case T(JAF_AND_ASSIGN, AIN_BOOL):        write_instruction0(state, ANDA); break;
	case T(JAF_LSHIFT_ASSIGN, AIN_INT):
	case T(JAF_LSHIFT_ASSIGN, AIN_BOOL):     write_instruction0(state, LSHIFTA); break;
	case T(JAF_RSHIFT_ASSIGN, AIN_INT):
	case T(JAF_RSHIFT_ASSIGN, AIN_BOOL):     write_instruction0(state, RSHIFTA); break;
	case T(JAF_CHAR_ASSIGN, AIN_INT):        write_instruction0(state, C_ASSIGN); break;
	case T(JAF_ASSIGN, AIN_LONG_INT):        write_instruction0(state, LI_ASSIGN); break;
	case T(JAF_ADD_ASSIGN, AIN_LONG_INT):    write_instruction0(state, LI_PLUSA); break;
	case T(JAF_SUB_ASSIGN, AIN_LONG_INT):    write_instruction0(state, LI_MINUSA); break;
	case T(JAF_MUL_ASSIGN, AIN_LONG_INT):    write_instruction0(state, LI_MULA); break;
	case T(JAF_DIV_ASSIGN, AIN_LONG_INT):    write_instruction0(state, LI_DIVA); break;
	case T(JAF_MOD_ASSIGN, AIN_LONG_INT):    write_instruction0(state, LI_MODA); break;
	case T(JAF_AND_ASSIGN, AIN_LONG_INT):    write_instruction0(state, LI_ANDA); break;
	case T(JAF_OR_ASSIGN, AIN_LONG_INT):     write_instruction0(state, LI_ORA); break;
	case T(JAF_XOR_ASSIGN, AIN_LONG_INT):    write_instruction0(state, LI_XORA); break;
	case T(JAF_LSHIFT_ASSIGN, AIN_LONG_INT): write_instruction0(state, LI_LSHIFTA); break;
	case T(JAF_RSHIFT_ASSIGN, AIN_LONG_INT): write_instruction0(state, LI_RSHIFTA); break;
	case T(JAF_ASSIGN, AIN_FLOAT):           write_instruction0(state, F_ASSIGN); break;
	case T(JAF_ADD_ASSIGN, AIN_FLOAT):       write_instruction0(state, F_PLUSA); break;
	case T(JAF_SUB_ASSIGN, AIN_FLOAT):       write_instruction0(state, F_MINUSA); break;
	case T(JAF_MUL_ASSIGN, AIN_FLOAT):       write_instruction0(state, F_MULA); break;
	case T(JAF_DIV_ASSIGN, AIN_FLOAT):       write_instruction0(state, F_DIVA); break;
	case T(JAF_ASSIGN, AIN_METHOD):          write_instruction0(state, DG_SET); break;
	case T(JAF_ASSIGN, AIN_DELEGATE):        write_instruction0(state, DG_ASSIGN); break;
	case T(JAF_ADD_ASSIGN, AIN_METHOD):      write_instruction0(state, DG_ADD); break;
	case T(JAF_ADD_ASSIGN, AIN_DELEGATE):    write_instruction0(state, DG_PLUSA); break;
	case T(JAF_SUB_ASSIGN, AIN_METHOD):      write_instruction0(state, DG_ERASE); break;
	case T(JAF_SUB_ASSIGN, AIN_DELEGATE):    write_instruction0(state, DG_MINUSA); break;
	case T(JAF_ASSIGN, AIN_STRUCT):
		write_instruction1(state, PUSH, rhs_type->struc);
		write_instruction0(state, SR_ASSIGN);
		break;
	case T(JAF_ASSIGN, AIN_STRING):
		switch (strip_ref(lhs_type)) {
		case AIN_FUNC_TYPE:
			write_instruction1(state, PUSH, lhs_type->struc);
			write_instruction0(state, FT_ASSIGNS);
			break;
		case AIN_DELEGATE:
			write_instruction1(state, PUSH, -1);
			write_instruction0(state, SWAP);
			write_instruction1(state, PUSH, lhs_type->struc);
			write_instruction0(state, DG_STR_TO_METHOD);
			write_instruction0(state, DG_SET);
			break;
		case AIN_STRING:
			write_instruction0(state, S_ASSIGN);
			break;
		default:
			_COMPILER_ERROR(NULL, -1, "Invalid string assignment");
		}
		break;
	case T(JAF_ADD_ASSIGN, AIN_STRING):
		switch (strip_ref(lhs_type)) {
		// FIXME: delegate
		case AIN_STRING:
			write_instruction0(state, S_PLUSA2);
			break;
		default:
			_COMPILER_ERROR(NULL, -1, "Invalid string assignment");
		}
		break;
	default:
		_COMPILER_ERROR(NULL, -1, "Invalid operator: %d", op);
	}
#undef T
}

static void compile_block(struct compiler_state *state, struct jaf_block *block);
static void compile_statement(struct compiler_state *state, struct jaf_block_item *item);
static void compile_expression(struct compiler_state *state, struct jaf_expression *expr);

static void compile_int(struct compiler_state *state, int i)
{
	write_instruction1(state, PUSH, i);
}

static void compile_float(struct compiler_state *state, float f)
{
	write_instruction1(state, F_PUSH, flo2int(f));
}

static void compile_string(struct compiler_state *state, const char *str)
{
	write_instruction1(state, S_PUSH, get_string_no(state, str));
}

static void compile_lock_peek(struct compiler_state *state)
{
	if (AIN_VERSION_LT(state->ain, 6, 0)) {
		write_instruction1(state, CALLSYS, SYS_LOCK_PEEK);
		write_instruction0(state, POP);
	}
}

static void compile_unlock_peek(struct compiler_state *state)
{
	if (AIN_VERSION_LT(state->ain, 6, 0)) {
		write_instruction1(state, CALLSYS, SYS_UNLOCK_PEEK);
		write_instruction0(state, POP);
	}
}

static struct ain_variable *get_local_variable(struct compiler_state *state, int var_no)
{
	assert(state->func_no >= 0 && state->func_no < state->ain->nr_functions);
	assert(var_no >= 0 && var_no < state->ain->functions[state->func_no].nr_vars);
	return &state->ain->functions[state->func_no].vars[var_no];
}

static struct ain_variable *get_global_variable(struct compiler_state *state, int var_no)
{
	assert(var_no >= 0 && var_no < state->ain->nr_globals);
	return &state->ain->globals[var_no];
}

static struct ain_variable *get_identifier_variable(struct compiler_state *state,
		struct jaf_expression *expr)
{
	switch (expr->ident.kind) {
	case JAF_IDENT_LOCAL:
		return get_local_variable(state, expr->ident.local->var);
	case JAF_IDENT_GLOBAL:
		return get_global_variable(state, expr->ident.global);
	case JAF_IDENT_CONST:
	case JAF_IDENT_UNRESOLVED:
		break;
	}
	COMPILER_ERROR(expr, "Unresolved variable");
}

static void compile_local_ref(struct compiler_state *state, int var_no)
{
	write_instruction0(state, PUSHLOCALPAGE);
	write_instruction1(state, PUSH, var_no);
}

static void compile_global_ref(struct compiler_state *state, int var_no)
{
	write_instruction0(state, PUSHGLOBALPAGE);
	write_instruction1(state, PUSH, var_no);
}

static void compile_identifier_ref(struct compiler_state *state, struct jaf_expression *expr)
{
	switch (expr->ident.kind) {
	case JAF_IDENT_LOCAL:
		compile_local_ref(state, expr->ident.local->var);
		break;
	case JAF_IDENT_GLOBAL:
		compile_global_ref(state, expr->ident.global);
		break;
	case JAF_IDENT_CONST:
	case JAF_IDENT_UNRESOLVED:
		COMPILER_ERROR(expr, "Unresolved variable");
	}
}

static void compile_lvalue_after(struct compiler_state *state, enum ain_data_type type)
{
	switch (type) {
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
	case AIN_IFACE:
		write_instruction0(state, REFREF);
		break;
	case AIN_STRING:
	case AIN_REF_STRING:
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			// nothing
		} else {
			write_instruction0(state, REF);
		}
		break;
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE:
	case AIN_STRUCT:
	case AIN_REF_STRUCT:
	case AIN_ARRAY:
	case AIN_REF_ARRAY:
	case AIN_DELEGATE:
		write_instruction0(state, REF);
		break;
	default:
		break;
	}
}

static void compile_lvalue(struct compiler_state *state, struct jaf_expression *expr);

static void compile_variable_ref(struct compiler_state *state, struct jaf_expression *expr)
{
	if (expr->type == JAF_EXP_IDENTIFIER) {
		compile_identifier_ref(state, expr);
	} else if (expr->type == JAF_EXP_MEMBER) {
		compile_lvalue(state, expr->member.struc);
		write_instruction1(state, PUSH, expr->member.member_no);
	} else if (expr->type == JAF_EXP_SUBSCRIPT) {
		compile_lvalue(state, expr->subscript.expr);  // page
		compile_expression(state, expr->subscript.index); // page-index
	} else {
		COMPILER_ERROR(expr, "Invalid lvalue (expression type %d)", expr->type);
	}
}

static void compile_function_arguments(struct compiler_state *state,
		struct jaf_argument_list *args, int func_no);
static void compile_cast(struct compiler_state *state, struct jaf_expression *expr);

static void compile_lvalue(struct compiler_state *state, struct jaf_expression *expr)
{
	switch (expr->type) {
	case JAF_EXP_IDENTIFIER: {
		struct ain_variable *v = get_identifier_variable(state, expr);
		switch (v->type.data) {
		case AIN_STRING:
		case AIN_ARRAY_TYPE:
		case AIN_STRUCT:
		case AIN_REF_STRING:
		case AIN_REF_ARRAY_TYPE:
		case AIN_REF_STRUCT:
			if (expr->ident.kind == JAF_IDENT_LOCAL)
				write_instruction1(state, SH_LOCALREF, expr->ident.local->var);
			else
				write_instruction1(state, SH_GLOBALREF, expr->ident.global);
			break;
		default:
			compile_identifier_ref(state, expr);
			compile_lvalue_after(state, v->type.data);
			break;
		}
		break;
	}
	case JAF_EXP_MEMBER:
		switch (expr->valuetype.data) {
		case AIN_STRING:
		case AIN_ARRAY_TYPE:
		case AIN_STRUCT:
		case AIN_DELEGATE:
		case AIN_REF_STRING:
		case AIN_REF_ARRAY_TYPE:
		case AIN_REF_STRUCT:
			if (expr->member.struc->type == JAF_EXP_THIS) {
				write_instruction1(state, SH_STRUCTREF, expr->member.member_no);
				break;
			}
			// fallthrough
		default:
			compile_lvalue(state, expr->member.struc);
			write_instruction1(state, PUSH, expr->member.member_no);
			compile_lvalue_after(state, expr->valuetype.data);
			break;
		}
		break;
	case JAF_EXP_SUBSCRIPT:
		compile_lvalue(state, expr->subscript.expr);  // page
		compile_expression(state, expr->subscript.index); // page-index
		compile_lvalue_after(state, expr->valuetype.data);
		break;
	case JAF_EXP_THIS:
		write_instruction0(state, PUSHSTRUCTPAGE);
		break;
	case JAF_EXP_NULL:
		write_instruction1(state, PUSH, -1);
		switch (expr->valuetype.data) {
		case AIN_REF_INT:
		case AIN_REF_FLOAT:
		case AIN_REF_BOOL:
		case AIN_REF_LONG_INT:
		case AIN_REF_ENUM:
		case AIN_IFACE:
			write_instruction1(state, PUSH, 0);
			break;
		case AIN_VOID:
			COMPILER_ERROR(expr, "Untyped NULL");
		default:
			break;
		}
		break;
	case JAF_EXP_DUMMYREF:
		// TODO? v11+ compiles call first (except for new)
		scope_add_variable(state, expr->dummy.var_no);
		// delete dummy variable
		if (AIN_VERSION_GTE(state->ain, 6, 1)) {
			write_instruction0(state, PUSHLOCALPAGE);
			write_instruction1(state, PUSH, expr->dummy.var_no);
			write_instruction0(state, REF);
			if (AIN_VERSION_GTE(state->ain, 12, 0)) {
				write_instruction0(state, DELETE);
			} else {
				// FIXME: this instruction appears starting in Evenicle.
				//        Rance 9 and Blade Briders don't use it, but are
				//        also 6.1
				write_instruction0(state, CHECKUDO);
			}
		}
		// prepare for assign to dummy variable
		compile_local_ref(state, expr->dummy.var_no);
		if (expr->dummy.expr->type == JAF_EXP_NEW) {
			// call constructor
			if (AIN_VERSION_GTE(state->ain, 11, 0)) {
				compile_function_arguments(state, expr->dummy.expr->new.args,
						expr->dummy.expr->new.func_no);
				write_instruction2(state, NEW, expr->valuetype.struc,
						expr->dummy.expr->new.func_no);
				write_instruction0(state, ASSIGN);
			} else {
				write_instruction1(state, PUSH, expr->valuetype.struc);
				compile_lock_peek(state);
				write_instruction0(state, NEW);
				write_instruction0(state, ASSIGN);
				compile_unlock_peek(state);
			}
		} else {
			compile_expression(state, expr->dummy.expr);
			if (is_ref_scalar(expr->valuetype.data) || expr->valuetype.data == AIN_IFACE) {
				write_instruction0(state, R_ASSIGN);
			} else {
				write_instruction0(state, ASSIGN);
			}
		}
		break;
	case JAF_EXP_CAST: {
		// cast of struct to interface
		// e.g. IFace foo() { return new Struct(); } // implicit cast
		//      ((IFace)new Struct()).iface_method();
		if (expr->valuetype.data != AIN_IFACE)
			COMPILER_ERROR(expr, "Invalid cast as lvalue");
		compile_cast(state, expr);
		break;
	}
	default:
		COMPILER_ERROR(expr, "Invalid lvalue (expression type %d)", expr->type);
	}
}

/*
 * Emit the code to put the value of a variable onto the stack (including member
 * variables and array elements). This code assumes a page + page-index is
 * already on the stack.
 */
static void compile_dereference(struct compiler_state *state, struct ain_type *type)
{
	switch (type->data) {
	case AIN_INT:
	case AIN_FLOAT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_FUNC_TYPE:
		write_instruction0(state, REF);
		break;
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
	case AIN_REF_FUNC_TYPE:
		write_instruction0(state, REFREF);
		write_instruction0(state, REF);
		break;
	case AIN_STRING:
	case AIN_REF_STRING:
		if (state->ain->version >= 11) {
			write_instruction0(state, REF);
			write_instruction0(state, A_REF);
		} else {
			write_instruction0(state, S_REF);
		}
		break;
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE:
	case AIN_ARRAY:
	case AIN_REF_ARRAY:
		write_instruction0(state, REF);
		write_instruction0(state, A_REF);
		break;
	case AIN_STRUCT:
	case AIN_REF_STRUCT:
		if (AIN_VERSION_GTE(state->ain, 11, 0)) {
			write_instruction0(state, REF);
			write_instruction0(state, A_REF);
		} else {
			write_instruction1(state, SR_REF, type->struc);
		}
		break;
	case AIN_DELEGATE:
	case AIN_REF_DELEGATE:
		write_instruction0(state, REF);
		write_instruction0(state, DG_COPY);
		break;
	default:
		_COMPILER_ERROR(NULL, -1, "Unsupported type: %d", type->data);
	}
}

static void compile_constant_identifier(struct compiler_state *state, struct jaf_expression *expr)
{
	switch (expr->ident.constval.data_type) {
	case AIN_INT:
		compile_int(state, expr->ident.constval.int_value);
		break;
	case AIN_FLOAT:
		compile_float(state, expr->ident.constval.float_value);
		break;
	case AIN_STRING:
		compile_string(state, expr->ident.constval.string_value);
		break;
	default:
		COMPILER_ERROR(expr, "Unhandled constant type");
	}
}

static void compile_identifier(struct compiler_state *state, struct jaf_expression *expr)
{
	assert(expr->ident.kind != JAF_IDENT_UNRESOLVED);
	if (expr->ident.kind == JAF_IDENT_CONST) {
		compile_constant_identifier(state, expr);
		return;
	}
	struct ain_variable *var = get_identifier_variable(state, expr);
	switch (var->type.data) {
	case AIN_INT:
	case AIN_FLOAT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_FUNC_TYPE:
		if (expr->ident.kind == JAF_IDENT_LOCAL)
			write_instruction1(state, SH_LOCALREF, expr->ident.local->var);
		else
			write_instruction1(state, SH_GLOBALREF, expr->ident.global);
		break;
	default:
		compile_identifier_ref(state, expr);
		compile_dereference(state, &var->type);
		break;
	}
}

/*
 * Pop a value of the given type off the stack.
 */
static void compile_pop(struct compiler_state *state, enum ain_data_type type)
{
	if (type == AIN_STRING || type == AIN_REF_STRING) {
		write_instruction0(state, AIN_VERSION_GTE(state->ain, 11, 0) ? DELETE : S_POP);
		return;
	}
	switch ((int)type) {
	case AIN_VOID:
		break;
	case AIN_INT:
	case AIN_FLOAT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_FUNC_TYPE:
	case AIN_REF_TYPE:
	case AIN_FUNCTION:
	case AIN_METHOD:
		write_instruction0(state, POP);
		break;
	case AIN_STRUCT:
		write_instruction0(state, SR_POP);
		break;
	case AIN_DELEGATE:
		write_instruction0(state, DG_POP);
		break;
	default:
		_COMPILER_ERROR(NULL, -1, "Unsupported type");
	}
}

static bool _compile_cast(struct compiler_state *state, struct jaf_expression *expr,
		struct ain_type *dst_t)
{
	enum ain_data_type src_type = expr->valuetype.data;
	enum ain_data_type dst_type = dst_t->data;

	if (src_type == dst_type)
		return true;
	if (src_type == AIN_INT) {
		if (dst_type == AIN_FLOAT) {
			write_instruction0(state, ITOF);
		} else if (dst_type == AIN_STRING) {
			write_instruction0(state, I_STRING);
		} else if (dst_type == AIN_LONG_INT) {
			write_instruction0(state, ITOLI);
		} else if (dst_type == AIN_BOOL) {
			write_instruction0(state, ITOB);
		} else {
			return false;
		}
	} else if (src_type == AIN_FLOAT) {
		if (dst_type == AIN_INT) {
			write_instruction0(state, FTOI);
		} else if (dst_type == AIN_STRING) {
			write_instruction1(state, PUSH, 6);
			write_instruction0(state, FTOS);
		} else if (dst_type == AIN_LONG_INT) {
			write_instruction0(state, FTOI);
			write_instruction0(state, ITOLI);
		} else if (dst_type == AIN_BOOL) {
			write_instruction0(state, FTOI);
			write_instruction0(state, ITOB);
		} else {
			return false;
		}
	} else if (src_type == AIN_STRING) {
		if (dst_type == AIN_INT) {
			write_instruction0(state, STOI);
		} else if (dst_type == AIN_LONG_INT) {
			write_instruction0(state, STOI);
			write_instruction0(state, ITOLI);
		} else {
			return false;
		}
	} else if (src_type == AIN_BOOL) {
		if (dst_type == AIN_INT || dst_type == AIN_LONG_INT) {
			/* nothing */
		} else if (dst_type == AIN_FLOAT) {
			write_instruction0(state, ITOF);
		}
	} else if (src_type == AIN_FUNCTION) {
		assert(dst_type == AIN_METHOD);
		write_instruction1(state, PUSH, -1);
		write_instruction0(state, SWAP);
	} else if (src_type == AIN_STRUCT || src_type == AIN_REF_STRUCT) {
		assert(dst_type == AIN_IFACE);
		int struct_no = expr->valuetype.struc;
		int iface_no = dst_t->struc;
		assert(struct_no >= 0 && struct_no < state->ain->nr_structures);
		assert(iface_no >= 0 && iface_no < state->ain->nr_structures);
		struct ain_struct *s = &state->ain->structures[struct_no];
		for (int i = 0; i < s->nr_interfaces; i++) {
			if (s->interfaces[i].struct_type == iface_no) {
				write_instruction1(state, PUSH, s->interfaces[i].vtable_offset);
				return true;
			}
		}
		return false;
	} else {
		return false;
	}
	return true;
}

static void compile_unary(struct compiler_state *state, struct jaf_expression *expr)
{
	switch (expr->op) {
	case JAF_AMPERSAND:
		write_instruction1(state, PUSH, expr->valuetype.struc);
		break;
	case JAF_UNARY_PLUS:
		compile_expression(state, expr->expr);
		break;
	case JAF_UNARY_MINUS:
		compile_expression(state, expr->expr);
		if (expr->valuetype.data == AIN_FLOAT)
			write_instruction0(state, F_INV);
		else
			write_instruction0(state, INV);
		break;
	case JAF_BIT_NOT:
		compile_expression(state, expr->expr);
		write_instruction0(state, COMPL);
		break;
	case JAF_LOG_NOT:
		compile_expression(state, expr->expr);
		write_instruction0(state, NOT);
		break;
	case JAF_PRE_INC:
		compile_lvalue(state, expr->expr);
		write_instruction0(state, DUP2);
		write_instruction0(state, INC);
		write_instruction0(state, REF);
		break;
	case JAF_PRE_DEC:
		compile_lvalue(state, expr->expr);
		write_instruction0(state, DUP2);
		write_instruction0(state, DEC);
		write_instruction0(state, REF);
		break;
	case JAF_POST_INC:
		compile_lvalue(state, expr->expr);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			write_instruction1(state, X_DUP, 2);
			write_instruction1(state, X_REF, 1);
			write_instruction2(state, X_MOV, 3, 1);
			write_instruction0(state, INC);
		} else {
			write_instruction0(state, DUP2);
			write_instruction0(state, REF);
			write_instruction0(state, DUP_X2);
			write_instruction0(state, POP);
			write_instruction0(state, INC);
		}
		break;
	case JAF_POST_DEC:
		compile_lvalue(state, expr->expr);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			write_instruction1(state, X_DUP, 2);
			write_instruction1(state, X_REF, 1);
			write_instruction2(state, X_MOV, 3, 1);
			write_instruction0(state, DEC);
		} else {
			write_instruction0(state, DUP2);
			write_instruction0(state, REF);
			write_instruction0(state, DUP_X2);
			write_instruction0(state, POP);
			write_instruction0(state, DEC);
		}
		break;
	default:
		COMPILER_ERROR(expr, "Invalid unary operator");
	}
}

static void compile_argument(struct compiler_state *state, struct jaf_expression *arg,
		enum ain_data_type type);

static void compile_property_assign(struct compiler_state *state, struct jaf_expression *expr)
{
	int setter_no = expr->lhs->member.setter_no;
	struct ain_function *m = &state->ain->functions[setter_no];
	compile_lvalue(state, expr->lhs->member.struc);
	write_instruction1(state, PUSH, setter_no);
	compile_argument(state, expr->rhs, m->vars[0].type.data);
	write_instruction0(state, DUP_X2);
	write_instruction1(state, CALLMETHOD, 1);
}

static void compile_binary(struct compiler_state *state, struct jaf_expression *expr)
{
	size_t addr[3];
	switch (expr->op) {
	case JAF_MULTIPLY:
	case JAF_DIVIDE:
	case JAF_REMAINDER:
	case JAF_PLUS:
	case JAF_MINUS:
	case JAF_LSHIFT:
	case JAF_RSHIFT:
	case JAF_LT:
	case JAF_GT:
	case JAF_LTE:
	case JAF_GTE:
	case JAF_EQ:
	case JAF_NEQ:
	case JAF_BIT_AND:
	case JAF_BIT_XOR:
	case JAF_BIT_IOR:
		compile_expression(state, expr->lhs);
		compile_expression(state, expr->rhs);
		write_instruction_for_arithmetic_op(state, expr->op, &expr->lhs->valuetype,
				&expr->rhs->valuetype);
		break;
	case JAF_REQ:
	case JAF_RNE:
		compile_lvalue(state, expr->lhs);
		compile_lvalue(state, expr->rhs);
		write_instruction_for_arithmetic_op(state, expr->op, &expr->lhs->valuetype,
				&expr->rhs->valuetype);
		break;
	case JAF_LOG_AND:
		compile_expression(state, expr->lhs);
		addr[0] = state->out.index + 2;
		write_instruction1(state, IFZ, 0);
		compile_expression(state, expr->rhs);
		addr[1] = state->out.index + 2;
		write_instruction1(state, IFZ, 0);
		write_instruction1(state, PUSH, 1);
		addr[2] = state->out.index + 2;
		write_instruction1(state, JUMP, 0);
		buffer_write_int32_at(&state->out, addr[0], state->out.index);
		buffer_write_int32_at(&state->out, addr[1], state->out.index);
		write_instruction1(state, PUSH, 0);
		buffer_write_int32_at(&state->out, addr[2], state->out.index);
		break;
	case JAF_LOG_OR:
		compile_expression(state, expr->lhs);
		addr[0] = state->out.index + 2;
		write_instruction1(state, IFNZ, 0);
		compile_expression(state, expr->rhs);
		addr[1] = state->out.index + 2;
		write_instruction1(state, IFNZ, 0);
		write_instruction1(state, PUSH, 0);
		addr[2] = state->out.index + 2;
		write_instruction1(state, JUMP, 0);
		buffer_write_int32_at(&state->out, addr[0], state->out.index);
		buffer_write_int32_at(&state->out, addr[1], state->out.index);
		write_instruction1(state, PUSH, 1);
		buffer_write_int32_at(&state->out, addr[2], state->out.index);
		break;
	case JAF_ASSIGN:
		if (expr->lhs->type == JAF_EXP_MEMBER && expr->lhs->member.type == JAF_DOT_PROPERTY) {
			compile_property_assign(state, expr);
			break;
		}
		// fallthrough
	case JAF_MUL_ASSIGN:
	case JAF_DIV_ASSIGN:
	case JAF_MOD_ASSIGN:
	case JAF_ADD_ASSIGN:
	case JAF_SUB_ASSIGN:
	case JAF_LSHIFT_ASSIGN:
	case JAF_RSHIFT_ASSIGN:
	case JAF_AND_ASSIGN:
	case JAF_XOR_ASSIGN:
	case JAF_OR_ASSIGN:
	case JAF_CHAR_ASSIGN:
		compile_lvalue(state, expr->lhs);
		if (AIN_VERSION_GTE(state->ain, 14, 0) && expr->lhs->valuetype.data == AIN_STRING) {
			write_instruction1(state, X_DUP, 2);
			write_instruction1(state, X_REF, 1);
			write_instruction0(state, DELETE);
		}
		compile_expression(state, expr->rhs);
		write_instruction_for_assignment_op(state, expr->op, &expr->lhs->valuetype,
				&expr->rhs->valuetype);
		break;
	default:
		COMPILER_ERROR(expr, "Invalid binary operator");
	}
}

static void compile_ternary(struct compiler_state *state, struct jaf_expression *expr)
{
	uint32_t addr[2];
	compile_expression(state, expr->condition);
	addr[0] = state->out.index + 2;
	write_instruction1(state, IFZ, 0);
	compile_expression(state, expr->consequent);
	addr[1] = state->out.index + 2;
	write_instruction1(state, JUMP, 0);
	buffer_write_int32_at(&state->out, addr[0], state->out.index);
	compile_expression(state, expr->alternative);
	buffer_write_int32_at(&state->out, addr[1], state->out.index);
}

/*
 * Emit the code for passing an argument to a function by reference.
 * FIXME: need to emit different code if argument is a reference-typed variable
 *        (e.g. REFREF instead of REF for 'ref int' variable)
 */
static void compile_reference_argument(struct compiler_state *state, struct jaf_expression *expr)
{
	// XXX: in 14+ there is a distinction between a string lvalue and a reference argument
	//      (string lvalue is a page+index, reference argument is just the string page)
	if (AIN_VERSION_GTE(state->ain, 14, 0)) {
		compile_lvalue(state, expr);
		if (expr->valuetype.data == AIN_STRING || expr->valuetype.data == AIN_REF_STRING) {
			write_instruction1(state, X_REF, 1);
		}
	} else {
		compile_lvalue(state, expr);
	}
}

static void compile_argument(struct compiler_state *state, struct jaf_expression *arg,
		enum ain_data_type type)
{
	if (ain_is_ref_data_type(type)) {
		compile_reference_argument(state, arg);
	} else if (type == AIN_DELEGATE) {
		compile_expression(state, arg);
		write_instruction0(state, DG_NEW_FROM_METHOD);
	} else {
		compile_expression(state, arg);
	}
}

static void compile_function_arguments(struct compiler_state *state,
		struct jaf_argument_list *args, int func_no)
{
	struct ain_function *f = &state->ain->functions[func_no];
	for (size_t i = 0; i < args->nr_items; i++) {
		compile_argument(state, args->items[i], f->vars[args->var_nos[i]].type.data);
	}
}

static void compile_interface_call_arguments(struct compiler_state *state,
		struct jaf_argument_list *args, int iface_no, int method_no)
{
	assert(iface_no >= 0 && iface_no < state->ain->nr_structures);
	struct ain_struct *s = &state->ain->structures[iface_no];
	assert(method_no >= 0 && method_no < s->nr_iface_methods);
	struct ain_function_type *f = &s->iface_methods[method_no];
	for (size_t i = 0; i < args->nr_items; i++) {
		compile_argument(state, args->items[i], f->variables[args->var_nos[i]].type.data);
	}
}

static void jaf_compile_functype_call(struct compiler_state *state, struct jaf_expression *expr)
{
	// NOTE: The functype expression has to be compiled first and then swapped with each
	//       argument in case one of the argument expressions changes the value of the
	//       functype expression.
	compile_expression(state, expr->call.fun);

	struct ain_function_type *f = &state->ain->function_types[expr->call.func_no];
	for (size_t i = 0; i < expr->call.args->nr_items; i++) {
		enum ain_data_type type = f->variables[expr->call.args->var_nos[i]].type.data;
		compile_argument(state, expr->call.args->items[i], type);
		if (is_ref_scalar(type)) {
			write_instruction0(state, DUP2_X1);
			write_instruction0(state, POP);
			write_instruction0(state, POP);
		} else {
			write_instruction0(state, SWAP);
		}
	}
	write_instruction1(state, PUSH, expr->call.fun->valuetype.struc);
	write_instruction0(state, CALLFUNC2);
}

static void jaf_compile_delegate_call(struct compiler_state *state, struct jaf_expression *expr)
{
	assert(expr->call.func_no >= 0 && expr->call.func_no < state->ain->nr_delegates);
	struct ain_function_type *dg = &state->ain->delegates[expr->call.func_no];
	compile_lvalue(state, expr->call.fun);
	for (size_t i = 0; i < expr->call.args->nr_items; i++) {
		enum ain_data_type type = dg->variables[expr->call.args->var_nos[i]].type.data;
		compile_argument(state, expr->call.args->items[i], type);
		write_instruction1(state, DG_CALLBEGIN, expr->call.func_no);
		size_t loop_addr = state->out.index;
		write_instruction2(state, DG_CALL, expr->call.func_no, 0);
		write_instruction1(state, JUMP, loop_addr);
		buffer_write_int32_at(&state->out, loop_addr + 6, state->out.index);
	}
}

static void compile_funcall(struct compiler_state *state, struct jaf_expression *expr)
{
	if (expr->call.fun->valuetype.data == AIN_FUNC_TYPE) {
		jaf_compile_functype_call(state, expr);
		return;
	} else if (expr->call.fun->valuetype.data == AIN_DELEGATE) {
		jaf_compile_delegate_call(state, expr);
		return;
	}
	compile_function_arguments(state, expr->call.args, expr->call.func_no);
	write_instruction1(state, CALLFUNC, expr->call.func_no);
}

static void _compile_method_call(struct compiler_state *state, struct jaf_expression *expr)
{
	if (AIN_VERSION_GTE(state->ain, 11, 0)) {
		write_instruction1(state, PUSH, expr->call.func_no);
		compile_function_arguments(state, expr->call.args, expr->call.func_no);
		// TODO: should this be f->nr_args or args->nr_items?
		write_instruction1(state, CALLMETHOD, expr->call.args->nr_items);
	} else {
		compile_function_arguments(state, expr->call.args, expr->call.func_no);
		write_instruction1(state, CALLMETHOD, expr->call.func_no);
	}
}

static void compile_method_call(struct compiler_state *state, struct jaf_expression *expr)
{
	assert(expr->call.fun->type == JAF_EXP_MEMBER);
	compile_lvalue(state, expr->call.fun->member.struc);
	_compile_method_call(state, expr);
}

static void compile_interface_call(struct compiler_state *state, struct jaf_expression *expr)
{
	assert(expr->call.fun->type == JAF_EXP_MEMBER);
	assert(expr->call.fun->valuetype.data == AIN_IMETHOD);
	struct jaf_expression *obj = expr->call.fun->member.struc;
	int method_no = expr->call.func_no;
	compile_lvalue(state, obj);
	write_instruction0(state, DUP_U2);
	write_instruction1(state, PUSH, 0);
	write_instruction0(state, REF);
	write_instruction0(state, SWAP);
	write_instruction1(state, PUSH, method_no);
	write_instruction0(state, ADD);
	write_instruction0(state, REF);
	compile_interface_call_arguments(state, expr->call.args, obj->valuetype.struc, method_no);
	write_instruction1(state, CALLMETHOD, expr->call.args->nr_items);
}

static void compile_syscall(struct compiler_state *state, struct jaf_expression *expr)
{
	unsigned nr_args = expr->call.args ? expr->call.args->nr_items : 0;
	for (unsigned i = 0; i < nr_args; i++) {
		compile_argument(state, expr->call.args->items[i], syscalls[expr->call.func_no].argtypes[i]);
	}

	if (state->ain->version >= 11) {
		JAF_ERROR(expr, "Syscalls not supported in ain v11");
	} else {
		write_instruction1(state, CALLSYS, expr->call.func_no);
	}
}

static void compile_hllcall(struct compiler_state *state, struct jaf_expression *expr)
{
	for (unsigned i = 0; i < expr->call.args->nr_items; i++) {
		struct ain_library *lib = &state->ain->libraries[expr->call.lib_no];
		compile_argument(state, expr->call.args->items[i], lib->functions[expr->call.func_no].arguments[i].type.data);
	}
	if (AIN_VERSION_GTE(state->ain, 11, 0)) {
		write_instruction3(state, CALLHLL, expr->call.lib_no, expr->call.func_no, expr->call.type_param);
	} else {
		write_instruction2(state, CALLHLL, expr->call.lib_no, expr->call.func_no);
	}
}

static void compile_super_call(struct compiler_state *state, struct jaf_expression *expr)
{
	assert(expr->call.fun->type == JAF_EXP_IDENTIFIER);
	if (state->ain->functions[expr->call.func_no].struct_type >= 0) {
		write_instruction0(state, PUSHSTRUCTPAGE);
		_compile_method_call(state, expr);
	} else {
		compile_function_arguments(state, expr->call.args, expr->call.func_no);
		write_instruction1(state, CALLFUNC, expr->call.func_no);
	}
}

static void compile_builtin_call(possibly_unused struct compiler_state *state, struct jaf_expression *expr)
{
	switch (expr->call.func_no) {
	case I_STRING:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, REF);
		write_instruction0(state, I_STRING);
		break;
	case FTOS:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, REF);
		write_instruction1(state, PUSH, -1);
		write_instruction0(state, FTOS);
		break;
	case STOI:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, S_REF);
		write_instruction0(state, STOI);
		break;
	case S_LENGTH:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, S_LENGTH);
		break;
	case S_LENGTHBYTE:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, S_LENGTHBYTE);
		break;
	case S_EMPTY:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, S_REF);
		write_instruction0(state, S_EMPTY);
		break;
	case S_FIND:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, S_REF);
		compile_expression(state, expr->call.args->items[0]);
		write_instruction0(state, S_FIND);
		break;
	case S_GETPART:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, S_REF);
		compile_expression(state, expr->call.args->items[0]);
		compile_expression(state, expr->call.args->items[1]);
		write_instruction0(state, S_GETPART);
		break;
	case S_PUSHBACK:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, REF);
		compile_expression(state, expr->call.args->items[0]);
		write_instruction0(state, S_PUSHBACK2);
		break;
	case S_POPBACK:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, REF);
		write_instruction0(state, S_POPBACK2);
		break;
	case S_ERASE:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, REF);
		compile_expression(state, expr->call.args->items[0]);
		write_instruction1(state, PUSH, 1); // number of chars?
		write_instruction0(state, S_ERASE2);
		break;
	case A_ALLOC:
		compile_variable_ref(state, expr->call.fun->member.struc);
		for (int i = 0; i < expr->call.args->nr_items; i++) {
			compile_expression(state, expr->call.args->items[i]);
		}
		write_instruction1(state, PUSH, expr->call.args->nr_items);
		write_instruction0(state, A_ALLOC);
		break;
	case A_REALLOC:
		compile_variable_ref(state, expr->call.fun->member.struc);
		for (int i = 0; i < expr->call.args->nr_items; i++) {
			compile_expression(state, expr->call.args->items[i]);
		}
		write_instruction1(state, PUSH, expr->call.args->nr_items);
		write_instruction0(state, A_REALLOC);
		break;
	case A_FREE:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, A_FREE);
		break;
	case A_NUMOF:
		compile_variable_ref(state, expr->call.fun->member.struc);
		if (expr->call.args->nr_items > 0) {
			compile_expression(state, expr->call.args->items[0]);
		} else {
			write_instruction1(state, PUSH, 1);
		}
		write_instruction0(state, A_NUMOF);
		break;
	case A_COPY:
		compile_variable_ref(state, expr->call.fun->member.struc);
		compile_expression(state, expr->call.args->items[0]);
		compile_expression(state, expr->call.args->items[1]);
		compile_expression(state, expr->call.args->items[2]);
		compile_expression(state, expr->call.args->items[3]);
		write_instruction0(state, A_COPY);
		break;
	case A_FILL:
		compile_variable_ref(state, expr->call.fun->member.struc);
		compile_expression(state, expr->call.args->items[0]);
		compile_expression(state, expr->call.args->items[1]);
		compile_expression(state, expr->call.args->items[2]);
		write_instruction0(state, A_FILL);
		break;
	case A_PUSHBACK:
		compile_variable_ref(state, expr->call.fun->member.struc);
		compile_expression(state, expr->call.args->items[0]);
		write_instruction0(state, A_PUSHBACK);
		break;
	case A_POPBACK:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, A_POPBACK);
		break;
	case A_EMPTY:
		compile_variable_ref(state, expr->call.fun->member.struc);
		write_instruction0(state, A_EMPTY);
		break;
	case A_ERASE:
		compile_variable_ref(state, expr->call.fun->member.struc);
		compile_expression(state, expr->call.args->items[0]);
		write_instruction0(state, A_ERASE);
		break;
	case A_INSERT:
		compile_variable_ref(state, expr->call.fun->member.struc);
		compile_expression(state, expr->call.args->items[0]);
		compile_expression(state, expr->call.args->items[1]);
		write_instruction0(state, A_INSERT);
		break;
	case A_SORT:
		compile_variable_ref(state, expr->call.fun->member.struc);
		if (expr->call.args->nr_items > 0) {
			compile_expression(state, expr->call.args->items[0]);
		} else {
			write_instruction1(state, PUSH, 0);
		}
		write_instruction0(state, A_SORT);
		break;
	case A_FIND:
		compile_variable_ref(state, expr->call.fun->member.struc);
		compile_expression(state, expr->call.args->items[0]);
		compile_expression(state, expr->call.args->items[1]);
		compile_expression(state, expr->call.args->items[2]);
		if (expr->call.args->nr_items > 3) {
			compile_expression(state, expr->call.args->items[3]);
		} else {
			write_instruction1(state, PUSH, 0);
		}
		write_instruction0(state, A_FIND);
		break;
	case DG_NUMOF:
		compile_lvalue(state, expr->call.fun->member.struc);
		write_instruction0(state, DG_NUMOF);
		break;
	case DG_EXIST:
		compile_lvalue(state, expr->call.fun->member.struc);
		compile_expression(state, expr->call.args->items[0]);
		write_instruction0(state, DG_EXIST);
	case DG_CLEAR:
		compile_lvalue(state, expr->call.fun->member.struc);
		write_instruction0(state, DG_CLEAR);
		break;
	default:
		JAF_ERROR(expr, "Unimplemented builtin method");
	}
}

static void compile_cast(struct compiler_state *state, struct jaf_expression *expr)
{
	compile_expression(state, expr->cast.expr);
	if (!_compile_cast(state, expr->cast.expr, &expr->valuetype)) {
		JAF_ERROR(expr, "Unsupported cast: %s to %s",
			ain_strtype(state->ain, expr->cast.expr->valuetype.data, -1),
			jaf_type_to_string(expr->cast.type));
	}
}

static void compile_member(struct compiler_state *state, struct jaf_expression *expr)
{
	compile_lvalue(state, expr->member.struc);
	switch (expr->member.type) {
	case JAF_DOT_MEMBER:
		write_instruction1(state, PUSH, expr->member.member_no);
		compile_dereference(state, &expr->valuetype);
		break;
	case JAF_DOT_METHOD:
		write_instruction1(state, PUSH, expr->valuetype.struc);
		break;
	case JAF_DOT_PROPERTY:
		write_instruction1(state, PUSH, expr->member.getter_no);
		write_instruction1(state, CALLMETHOD, 0);
		break;
	}
}

static void compile_seq(struct compiler_state *state, struct jaf_expression *expr)
{
	compile_expression(state, expr->seq.head);
	compile_pop(state, expr->seq.head->valuetype.data);
	compile_expression(state, expr->seq.tail);
}

static void compile_subscript(struct compiler_state *state, struct jaf_expression *expr)
{
	compile_lvalue(state, expr->subscript.expr);
	compile_expression(state, expr->subscript.index);
	if (expr->subscript.expr->valuetype.data == AIN_STRING) {
		write_instruction0(state, C_REF);
	} else {
		compile_dereference(state, &expr->valuetype);
	}
}

static void compile_expression(struct compiler_state *state, struct jaf_expression *expr)
{
	switch (expr->type) {
	case JAF_EXP_VOID:
		break;
	case JAF_EXP_INT:
		compile_int(state, expr->i);
		break;
	case JAF_EXP_FLOAT:
		compile_float(state, expr->f);
		break;
	case JAF_EXP_STRING:
		compile_string(state, expr->s->text);
		break;
	case JAF_EXP_IDENTIFIER:
		compile_identifier(state, expr);
		break;
	case JAF_EXP_THIS:
		write_instruction0(state, PUSHSTRUCTPAGE);
		break;
	case JAF_EXP_UNARY:
		compile_unary(state, expr);
		break;
	case JAF_EXP_BINARY:
		compile_binary(state, expr);
		break;
	case JAF_EXP_TERNARY:
		compile_ternary(state, expr);
		break;
	case JAF_EXP_FUNCALL:
		compile_funcall(state, expr);
		break;
	case JAF_EXP_SYSCALL:
		compile_syscall(state, expr);
		break;
	case JAF_EXP_HLLCALL:
		compile_hllcall(state, expr);
		break;
	case JAF_EXP_METHOD_CALL:
		compile_method_call(state, expr);
		break;
	case JAF_EXP_INTERFACE_CALL:
		compile_interface_call(state, expr);
		break;
	case JAF_EXP_SUPER_CALL:
		compile_super_call(state, expr);
		break;
	case JAF_EXP_BUILTIN_CALL:
		compile_builtin_call(state, expr);
		break;
	case JAF_EXP_NEW:
		COMPILER_ERROR(expr, "bare new expression");
		break;
	case JAF_EXP_CAST:
		compile_cast(state, expr);
		break;
	case JAF_EXP_MEMBER:
		compile_member(state, expr);
		break;
	case JAF_EXP_SEQ:
		compile_seq(state, expr);
		break;
	case JAF_EXP_SUBSCRIPT:
		compile_subscript(state, expr);
		break;
	case JAF_EXP_CHAR:
		COMPILER_ERROR(expr, "Unresolved character constant"); // should have been simplified to int
		break;
	case JAF_EXP_NULL:
		switch (expr->valuetype.data) {
		case AIN_FUNC_TYPE:
		case AIN_IMAIN_SYSTEM:
			write_instruction1(state, PUSH, 0);
			break;
		case AIN_DELEGATE:
			write_instruction0(state, DG_NEW);
			break;
		case AIN_STRING:
			write_instruction1(state, S_PUSH, 0);
			break;
		default:
			COMPILER_ERROR(expr, "Unimplemented NULL rvalue type: %d", expr->valuetype.data);
			break;
		}
		break;
	case JAF_EXP_DUMMYREF:
		compile_lvalue(state, expr);
		break;
	}
}

static void compile_expr_and_pop(struct compiler_state *state, struct jaf_expression *expr)
{
	switch (expr->type) {
	case JAF_EXP_BINARY:
		if (expr->op == JAF_ASSIGN
				&& expr->lhs->type == JAF_EXP_IDENTIFIER
				&& expr->lhs->ident.kind == JAF_IDENT_LOCAL
				&& expr->rhs->type == JAF_EXP_INT) {
			int var_no = expr->lhs->ident.local->var;
			struct ain_variable *v = get_local_variable(state, var_no);
			if (!ain_is_ref_data_type(v->type.data)) {
				write_instruction2(state, SH_LOCALASSIGN, var_no, expr->rhs->i);
				return;
			}
		}
		break;
	case JAF_EXP_UNARY:
		switch (expr->op) {
		case JAF_PRE_INC:
		case JAF_PRE_DEC:
		case JAF_POST_INC:
		case JAF_POST_DEC:
			if (expr->expr->type == JAF_EXP_IDENTIFIER
					&& expr->expr->ident.kind == JAF_IDENT_LOCAL) {
				int var_no = expr->expr->ident.local->var;
				struct ain_variable *v = get_local_variable(state, var_no);
				if (!ain_is_ref_data_type(v->type.data)) {
					if (expr->op == JAF_PRE_INC || expr->op == JAF_POST_INC)
						write_instruction1(state, SH_LOCALINC, var_no);
					else
						write_instruction1(state, SH_LOCALDEC, var_no);
					return;
				}
			}
			if (expr->op == JAF_PRE_INC || expr->op == JAF_PRE_DEC) {
				compile_lvalue(state, expr->expr);
				write_instruction0(state, DUP2);
				write_instruction0(state, incdec_instruction(expr->op,
							expr->expr->valuetype.data));
				write_instruction0(state, POP);
				write_instruction0(state, POP);
				return;
			}
			break;
		default:
			break;
		}
		break;
	case JAF_EXP_SEQ:
		compile_expr_and_pop(state, expr->seq.head);
		compile_expr_and_pop(state, expr->seq.tail);
		return;
	default:
		break;
	}
	compile_expression(state, expr);
	compile_pop(state, expr->valuetype.data);
}

static void compile_vardecl(struct compiler_state *state, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	if (decl->type->qualifiers & JAF_QUAL_CONST) {
		return;
	}

	struct jaf_expression lhs = {
		.line = item->line,
		.file = item->file,
		.type = JAF_EXP_IDENTIFIER,
		.valuetype = decl->valuetype,
		.ident = {
			.name = decl->name,
			.kind = JAF_IDENT_LOCAL,
			.local = decl
		}
	};
	struct jaf_expression rhs = {
		.line = item->line,
		.file = item->file,
		// .type = <filled in later>
		.valuetype = decl->valuetype,
	};
	struct jaf_expression assign = {
		.line = item->line,
		.file = item->file,
		.type = JAF_EXP_BINARY,
		.op = JAF_ASSIGN,
		.valuetype = decl->valuetype,
		.lhs = &lhs,
		.rhs = &rhs,
	};
	switch (decl->valuetype.data) {
	case AIN_VOID:
		COMPILER_ERROR(item, "void variable declaration");
	case AIN_REF_TYPE:
	case AIN_IFACE: {
		struct jaf_block_item ref_assign = {
			.line = item->line,
			.file = item->file,
			.kind = JAF_STMT_RASSIGN,
			.rassign = {
				.lhs = &lhs,
				.rhs = &rhs
			}
		};
		if (decl->init) {
			ref_assign.rassign.rhs = decl->init;
		} else {
			rhs.type = JAF_EXP_NULL;
		}
		compile_statement(state, &ref_assign);
		break;
	}
	case AIN_STRING:
		// XXX: string variable is deleted first on v14+
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			compile_local_ref(state, decl->var);
			write_instruction1(state, X_DUP, 2);
			write_instruction1(state, X_REF, 1);
			write_instruction0(state, DELETE);
			if (decl->init) {
				compile_expression(state, decl->init);
			} else {
				write_instruction1(state, S_PUSH, 0);
			}
			write_instruction1(state, X_ASSIGN, 1);
			write_instruction0(state, POP);
			break;
		}
		// fallthrough
	case AIN_INT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_FLOAT:
	case AIN_FUNC_TYPE:
		if (decl->init) {
			assign.rhs = decl->init;
		} else {
			switch (decl->valuetype.data) {
			case AIN_INT:
			case AIN_BOOL:
			case AIN_LONG_INT:
				rhs.type = JAF_EXP_INT;
				rhs.i = 0;
				break;
			case AIN_FLOAT:
				rhs.type = JAF_EXP_FLOAT;
				rhs.f = 0.f;
				break;
			case AIN_FUNC_TYPE:
			case AIN_STRING:
				rhs.type = JAF_EXP_NULL;
				break;
			default:
				COMPILER_ERROR(item, "unreachable");
			}
		}
		compile_expr_and_pop(state, &assign);
		break;
	case AIN_STRUCT:
		write_instruction1(state, SH_LOCALDELETE, decl->var);
		write_instruction2(state, SH_LOCALCREATE, decl->var, decl->valuetype.struc);
		if (decl->init) {
			compile_local_ref(state, decl->var);
			write_instruction0(state, REF);
			compile_expression(state, decl->init);
			write_instruction1(state, PUSH, decl->valuetype.struc);
			write_instruction0(state, SR_ASSIGN);
			write_instruction0(state, SR_POP);
		}
		break;
	case AIN_ARRAY:
	case AIN_ARRAY_TYPE:
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			compile_local_ref(state, decl->var);
			write_instruction1(state, X_DUP, 2);
			write_instruction1(state, X_REF, 1);
			write_instruction0(state, DELETE);
			if (decl->init) {
				if (decl->array_dims) {
					// FIXME: need JAF_WARNING here
					WARNING("Initializer provided; ignoring array dimensions");
				}
				compile_expression(state, decl->init);
				write_instruction1(state, X_ASSIGN, 1);
				write_instruction0(state, POP);
			} else {
				write_instruction1(state, PUSH, 0);
				write_instruction1(state, X_A_INIT, 0);
				write_instruction0(state, POP);
				if (decl->array_dims) {
					if (decl->type->rank != 1) {
						JAF_ERROR(item, "Only rank-1 arrays supported on ain v14+");
					}
					compile_local_ref(state, decl->var);
					write_instruction0(state, REF);
					write_instruction1(state, PUSH, decl->array_dims[0]->i);
					write_CALLHLL(state, "Array", "Alloc", 1); // ???
				}
			}
		} else if (AIN_VERSION_GTE(state->ain, 11, 0)) {
			compile_local_ref(state, decl->var);
			write_instruction0(state, REF);
			if (decl->init) {
				if (decl->array_dims) {
					// FIXME: need JAF_WARNING here
					WARNING("Initializer provided; ignoring array dimensions");
				}
				compile_expression(state, decl->init);
				write_instruction0(state, X_SET);
				write_instruction0(state, DELETE);
			} else if (decl->array_dims) {
				if (decl->type->rank != 1) {
					JAF_ERROR(item, "Only rank-1 arrays supported on ain v11+");
				}
				write_instruction0(state, DUP);
				write_instruction1(state, PUSH, decl->array_dims[0]->i);
				write_instruction1(state, PUSH, -1);
				write_instruction1(state, PUSH, -1);
				write_instruction1(state, PUSH, -1);
				write_CALLHLL(state, "Array", "Alloc", decl->valuetype.array_type->data);
			} else {
				write_CALLHLL(state, "Array", "Free", decl->valuetype.array_type->data);
			}
		} else {
			compile_local_ref(state, decl->var);
			if (decl->array_dims) {
				for (size_t i = 0; i < decl->type->rank; i++) {
					write_instruction1(state, PUSH, decl->array_dims[i]->i);
				}
				write_instruction1(state, PUSH, decl->type->rank);
				write_instruction0(state, A_ALLOC);
			} else {
				write_instruction0(state, A_FREE);
			}
		}
		break;
	case AIN_DELEGATE:
		compile_local_ref(state, decl->var);
		write_instruction0(state, REF);
		if (decl->init) {
			compile_expression(state, decl->init);
			if (decl->init->valuetype.data == AIN_STRING) {
				write_instruction1(state, PUSH, -1);
				write_instruction0(state, SWAP);
				write_instruction1(state, PUSH, decl->valuetype.struc);
				write_instruction0(state, DG_STR_TO_METHOD);
			}
			write_instruction0(state, DG_SET);
		} else {
			write_instruction0(state, DG_CLEAR);
		}
		break;
	default:
		COMPILER_ERROR(item, "Unsupported variable type: %d", decl->valuetype.data);
	}

	switch (decl->valuetype.data) {
	case AIN_STRUCT:
	case AIN_ARRAY:
	case AIN_ARRAY_TYPE:
	case AIN_IFACE:
	case AIN_REF_TYPE:
		scope_add_variable(state, decl->var);
		break;
	default:
		break;
	}
}

static void compile_if(struct compiler_state *state, struct jaf_expression *test, struct jaf_block_item *then, struct jaf_block_item *alt)
{
	uint32_t addr[3];
	compile_expression(state, test);
	addr[0] = state->out.index + 2;
	write_instruction1(state, IFNZ, 0);
	addr[1] = state->out.index + 2;
	write_instruction1(state, JUMP, 0);
	buffer_write_int32_at(&state->out, addr[0], state->out.index);
	compile_statement(state, then);
	if (alt) {
		addr[2] = state->out.index + 2;
		write_instruction1(state, JUMP, 0);
		buffer_write_int32_at(&state->out, addr[1], state->out.index);
		compile_statement(state, alt);
		buffer_write_int32_at(&state->out, addr[2], state->out.index);
	} else {
		buffer_write_int32_at(&state->out, addr[1], state->out.index);
	}
}

static void compile_while(struct compiler_state *state, struct jaf_expression *test, struct jaf_block_item *body)
{
	uint32_t addr;
	// loop test
	start_loop(state);
	compile_expression(state, test);
	addr = state->out.index + 2;
	write_instruction1(state, IFZ, 0);
	// loop body
	compile_statement(state, body);
	write_instruction1(state, JUMP, state->loops[state->nr_loops-1].loop_addr);
	// loop end
	buffer_write_int32_at(&state->out, addr, state->out.index);
	end_loop(state);
}

static void compile_do_while(struct compiler_state *state, struct jaf_expression *test, struct jaf_block_item *body)
{
	uint32_t addr[2];
	// skip loop test
	addr[0] = state->out.index + 2;
	write_instruction1(state, JUMP, 0);
	// loop test
	start_loop(state);
	compile_expression(state, test);
	addr[1] = state->out.index + 2; // address of address of loop end
	write_instruction1(state, IFZ, 0);
	// loop body
	buffer_write_int32_at(&state->out, addr[0], state->out.index);
	compile_statement(state, body);
	write_instruction1(state, JUMP, state->loops[state->nr_loops-1].loop_addr);
	// loop end
	buffer_write_int32_at(&state->out, addr[1], state->out.index);
	end_loop(state);
}

static void compile_for(struct compiler_state *state, struct jaf_block *init, struct jaf_expression *test, struct jaf_expression *after, struct jaf_block_item *body)
{
	uint32_t addr[3];
	// loop init
	compile_block(state, init);
	// loop test
	addr[0] = state->out.index; // address of loop test
	if (test)
		compile_expression(state, test);
	else
		write_instruction1(state, PUSH, 1);
	addr[1] = state->out.index + 2; // address of address of loop end
	write_instruction1(state, IFZ, 0);
	addr[2] = state->out.index + 2; // address of address of loop body
	write_instruction1(state, JUMP, 0);
	// loop increment
	start_loop(state);
	if (after)
		compile_expr_and_pop(state, after);
	write_instruction1(state, JUMP, addr[0]);
	// loop body
	buffer_write_int32_at(&state->out, addr[2], state->out.index);
	compile_statement(state, body);
	write_instruction1(state, JUMP, state->loops[state->nr_loops-1].loop_addr);
	// loop end
	buffer_write_int32_at(&state->out, addr[1], state->out.index);
	end_loop(state);
}

static void compile_label(struct compiler_state *state, struct jaf_block_item *item)
{
	// add label
	state->labels = xrealloc_array(state->labels, state->nr_labels, state->nr_labels+1, sizeof(struct label));
	state->labels[state->nr_labels].name = item->label.name->text;
	state->labels[state->nr_labels].addr = state->out.index;
	state->nr_labels++;

	compile_statement(state, item->label.stmt);
}

static void compile_goto(struct compiler_state *state, struct jaf_block_item *item)
{
	// add goto
	state->gotos = xrealloc_array(state->gotos, state->nr_gotos, state->nr_gotos+1, sizeof(struct label));
	state->gotos[state->nr_gotos].name = item->label.name->text;
	state->gotos[state->nr_gotos].addr = state->out.index + 2;
	state->nr_gotos++;

	write_instruction1(state, JUMP, 0);
}

static void compile_break(struct compiler_state *state, struct jaf_block_item *item)
{
	if (state->nr_loops == 0)
		JAF_ERROR(item, "break outside of loop");
	struct loop_state *loop = &state->loops[state->nr_loops-1];
	loop->breaks = xrealloc_array(loop->breaks, loop->nr_breaks, loop->nr_breaks+1, sizeof(uint32_t));
	loop->breaks[loop->nr_breaks++] = state->out.index + 2;
	write_instruction1(state, JUMP, 0);
}

static void compile_message(struct compiler_state *state, struct jaf_block_item *item)
{
	char *msg = conv_output(item->msg.text->text);
	int no = ain_add_message(state->ain, msg);
	free(msg);
	write_instruction1(state, _MSG, no);
	if (item->msg.func)
		write_instruction1(state, CALLFUNC, item->msg.func_no);
}

static void compile_rassign(struct compiler_state *state, struct jaf_block_item *item)
{
	// delete previous reference
	compile_lock_peek(state);
	compile_variable_ref(state, item->rassign.lhs);
	write_instruction0(state, DUP2);
	write_instruction0(state, REF);
	write_instruction0(state, DELETE);

	if (item->rassign.rhs->type != JAF_EXP_NULL)
		write_instruction0(state, DUP2);

	compile_lvalue(state, item->rassign.rhs);

	switch (item->rassign.lhs->valuetype.data) {
	case AIN_REF_SCALAR_TYPE:
	case AIN_IFACE:
		// NOTE: SDK compiler emits [DUP_U2; SP_INC; R_ASSIGN; POP; POP] here
		write_instruction0(state, R_ASSIGN);
		write_instruction0(state, POP);
		if (item->rassign.rhs->type == JAF_EXP_NULL) {
			write_instruction0(state, POP);
		} else {
			write_instruction0(state, POP);
			write_instruction0(state, REF);
			write_instruction0(state, SP_INC);
		}
		break;
	case AIN_REF_STRING:
	case AIN_REF_STRUCT:
	case AIN_REF_ARRAY_TYPE:
	case AIN_REF_ARRAY:
		// NOTE: SDK compiler emits [DUP; SP_INC; ASSIGN; POP] here
		write_instruction0(state, ASSIGN);
		if (item->rassign.rhs->type == JAF_EXP_NULL) {
			write_instruction0(state, POP);
		} else {
			write_instruction0(state, DUP_X2);
			write_instruction0(state, POP);
			write_instruction0(state, REF);
			write_instruction0(state, SP_INC);
			write_instruction0(state, POP);
		}
		break;
	default:
		COMPILER_ERROR(item, "Invalid LHS in reference assignment");
	}
	compile_unlock_peek(state);
}

static void compile_return(struct compiler_state *state, struct jaf_block_item *item)
{
	if (!item->expr) {
		write_instruction0(state, RETURN);
		return;
	}

	switch (state->ain->functions[state->func_no].return_type.data) {
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_LONG_INT:
	case AIN_REF_BOOL:
	case AIN_REF_FUNC_TYPE:
	case AIN_IFACE:
		compile_lvalue(state, item->expr);
		write_instruction0(state, DUP_U2);
		write_instruction0(state, SP_INC);
		break;
	case AIN_REF_STRING:
	case AIN_REF_STRUCT:
	case AIN_REF_ARRAY_TYPE:
		compile_lvalue(state, item->expr);
		write_instruction0(state, DUP);
		write_instruction0(state, SP_INC);
		break;
	default:
		if (ain_is_ref_data_type(item->expr->valuetype.data))
			COMPILER_ERROR(item, "Unimplemented ref type as return value");
		compile_expression(state, item->expr);
		break;
	}
	write_instruction0(state, RETURN);
}

static void compile_statement(struct compiler_state *state, struct jaf_block_item *item)
{
	if (item->is_scope) {
		start_scope(state);
	}

	for (int i = (int)kv_size(item->delete_vars) - 1; i >= 0; i--) {
		compile_delete_var(state, kv_A(item->delete_vars, i)->var);
	}

	switch (item->kind) {
	case JAF_DECL_VAR:
		compile_vardecl(state, item);
		break;
	case JAF_DECL_FUNCTYPE:
		JAF_ERROR(item, "Function types must be declared at top-level");
	case JAF_DECL_DELEGATE:
		JAF_ERROR(item, "Delegates must be declared at top-level");
	case JAF_DECL_FUN:
		JAF_ERROR(item, "Functions must be defined at top-level");
	case JAF_DECL_STRUCT:
		JAF_ERROR(item, "Structs must be defined at top-level");
	case JAF_DECL_INTERFACE:
		JAF_ERROR(item, "Interfaces must be defined at top-level");
	case JAF_STMT_NULL:
		break;
	case JAF_STMT_LABELED:
		compile_label(state, item);
		break;
	case JAF_STMT_COMPOUND:
		compile_block(state, item->block);
		break;
	case JAF_STMT_EXPRESSION:
		compile_expr_and_pop(state, item->expr);
		break;
	case JAF_STMT_IF:
		compile_if(state, item->cond.test, item->cond.consequent, item->cond.alternative);
		break;
	case JAF_STMT_SWITCH:
		COMPILER_ERROR(item, "switch not supported");
		break;
	case JAF_STMT_WHILE:
		compile_while(state, item->while_loop.test, item->while_loop.body);
		break;
	case JAF_STMT_DO_WHILE:
		compile_do_while(state, item->while_loop.test, item->while_loop.body);
		break;
	case JAF_STMT_FOR:
		compile_for(state, item->for_loop.init, item->for_loop.test, item->for_loop.after, item->for_loop.body);
		break;
	case JAF_STMT_GOTO:
		compile_goto(state, item);
		break;
	case JAF_STMT_CONTINUE:
		if (state->nr_loops == 0)
			JAF_ERROR(item, "continue outside of loop");
		write_instruction1(state, JUMP, state->loops[state->nr_loops-1].loop_addr);
		break;
	case JAF_STMT_BREAK:
		compile_break(state, item);
		break;
	case JAF_STMT_RETURN:
		compile_return(state, item);
		break;
	case JAF_STMT_CASE:
		COMPILER_ERROR(item, "switch not supported");
		break;
	case JAF_STMT_DEFAULT:
		COMPILER_ERROR(item, "switch not supported");
		break;
	case JAF_STMT_MESSAGE:
		compile_message(state, item);
		break;
	case JAF_STMT_RASSIGN:
		compile_rassign(state, item);
		break;
	case JAF_STMT_ASSERT:
		compile_expression(state, item->assertion.expr);
		compile_expression(state, item->assertion.expr_string);
		compile_expression(state, item->assertion.file);
		write_instruction1(state, PUSH, item->assertion.line);
		write_instruction0(state, ASSERT);
		break;
	case JAF_EOF:
		write_instruction1(state, _EOF, item->file_no);
		break;
	}

	if (item->is_scope) {
		end_scope(state);
	}
}

static void compile_block(struct compiler_state *state, struct jaf_block *block)
{
	for (size_t i = 0; i < block->nr_items; i++) {
		compile_statement(state, block->items[i]);
	}
}

static void compile_default_return(struct compiler_state *state, enum ain_data_type type)
{
	switch (type) {
	case AIN_VOID:
		break;
	case AIN_INT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_ENUM:
		write_instruction1(state, PUSH, 0);
		break;
	case AIN_FLOAT:
		write_instruction1(state, F_PUSH, 0);
		break;
	case AIN_STRING:
		write_instruction1(state, S_PUSH, 0);
		break;
	case AIN_STRUCT:
	case AIN_REF_STRUCT:
	case AIN_REF_STRING:
	case AIN_ARRAY:
	case AIN_REF_ARRAY:
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE:
	case AIN_DELEGATE:
		write_instruction1(state, PUSH, -1);
		break;
	case AIN_REF_INT:
	case AIN_REF_BOOL:
	case AIN_REF_FLOAT:
	case AIN_REF_LONG_INT:
	case AIN_IFACE:
		write_instruction1(state, PUSH, -1);
		write_instruction1(state, PUSH, 0);
		break;
	default:
		_COMPILER_ERROR(NULL, -1, "Unsupported type: %s", ain_strtype(state->ain, type, -1));
	}
}

static int get_alloc_fun(struct ain *ain, struct ain_function *ctor)
{
	char *alloc_name = xstrdup(ctor->name);
	size_t len = strlen(alloc_name);
	assert(len > 0);
	assert(alloc_name[len-1] == '0');
	alloc_name[len-1] = '2';
	int no = ain_get_function(ain, alloc_name);
	free(alloc_name);
	return no;
}

static void compile_function(struct compiler_state *state, struct jaf_fundecl *decl)
{
	assert(decl->func_no >= 0 && decl->func_no < state->ain->nr_functions);
	struct ain_function *f = &state->ain->functions[decl->func_no];
	f->crc = 0;

	start_function(state, decl->func_no, decl->super_no);
	write_instruction1(state, FUNC, decl->func_no);
	f->address = state->out.index;
	if (decl->fun_type == JAF_FUN_CONSTRUCTOR) {
		int alloc_no = get_alloc_fun(state->ain, f);
		if (alloc_no > 0) {
			write_instruction0(state, PUSHSTRUCTPAGE);
			write_instruction1(state, CALLMETHOD, alloc_no);
		}
	}
	compile_block(state, decl->body);
	compile_default_return(state, f->return_type.data);
	write_instruction0(state, RETURN);
	write_instruction1(state, ENDFUNC, decl->func_no);
	end_function(state);
}

static void compile_declaration(struct compiler_state *state, struct jaf_block_item *decl)
{
	if (decl->kind == JAF_DECL_FUN && decl->fun.body) {
		compile_function(state, &decl->fun);
		return;
	}
	if (decl->kind == JAF_DECL_STRUCT) {
		for (size_t i = 0; i < decl->struc.methods->nr_items; i++) {
			compile_declaration(state, decl->struc.methods->items[i]);
		}
	}
	if (decl->kind == JAF_EOF) {
		compile_statement(state, decl);
	}

	// TODO: global constructors/array allocations
}

static void compile_global_init_function(struct compiler_state *state)
{
	if (ain_get_function(state->ain, "0") >= 0)
		return;

	int func_no = ain_add_function(state->ain, "0");
	state->ain->functions[func_no].address = state->out.index + 6;
	state->ain->functions[func_no].return_type = (struct ain_type) {
		.data = AIN_VOID,
		.struc = -1,
		.rank = 0
	};

	// TODO: actually implement this
	write_instruction1(state, FUNC, func_no);
	write_instruction0(state, RETURN);
	write_instruction1(state, ENDFUNC, func_no);
}

static void jaf_compile(struct ain *ain, struct jaf_block *toplevel)
{
	assert(toplevel->nr_items > 0);
	struct compiler_state state = {
		.ain = ain,
		.out = {
			.buf = ain->code,
			.size = ain->code_size,
			.index = ain->code_size
		},
	};
	for (size_t i = 0; i < toplevel->nr_items - 1; i++) {
		compile_declaration(&state, toplevel->items[i]);
	}
	compile_global_init_function(&state);

	// add NULL function
	write_instruction1(&state, FUNC, 0);
	state.ain->functions[0].address = state.out.index;

	// add final EOF
	compile_declaration(&state, toplevel->items[toplevel->nr_items-1]);

	free(state.loops);
	free(state.scopes);
	ain->code = state.out.buf;
	ain->code_size = state.out.index;

	// XXX: ain_add_initval adds initval to GSET section of the ain file.
	//      If the original ain file did not have a GSET section, init
	//      code needs to be added to the "0" function instead.
	if (ain->nr_initvals && !ain->GSET.present)
		WARNING("%d global initvals ignored", ain->nr_initvals);
}

void jaf_build(struct ain *out, const char **files, unsigned nr_files, const char **hll, unsigned nr_hll)
{
	// First, we parse the source files and register type definitions in the ain file.
	struct jaf_block *toplevel;
	// pass 0: parse (type names registered in ain object here)
	toplevel = jaf_parse(out, files, nr_files);
	// pass 1: resolve typedefs
	jaf_resolve_types(out, toplevel);
	// pass 2: globals/functions
	jaf_process_declarations(out, toplevel);

	// Now that type definitions are available, we parse the HLL files.
	assert(nr_hll % 2 == 0);
	for (unsigned i = 0; i < nr_hll; i += 2) {
		struct jaf_block *hll_decl = jaf_parse(out, hll+i, 1);
		jaf_process_hll_declarations(out, hll_decl, hll[i+1]);
		jaf_free_block(hll_decl);
	}

	// pass 3: static analysis (type analysis, simplification, global initvals)
	toplevel = jaf_static_analyze(out, toplevel);

	// pass 4: allocate local variables
	jaf_allocate_variables(out, toplevel);

	jaf_compile(out, toplevel);
	jaf_free_block(toplevel);
}
