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
#include "ainedit.h"
#include "jaf.h"
#include "jaf_parser.tab.h"

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
	size_t nr_vars;
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
	if (state->ain->version >= 14) {
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
	write_opcode(state, opcode);
	write_argument(state, arg0);
}

static void write_instruction2(struct compiler_state *state, uint16_t opcode, uint32_t arg0, uint32_t arg1)
{
	write_opcode(state, opcode);
	write_argument(state, arg0);
	write_argument(state, arg1);
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

static void end_scope(struct compiler_state *state)
{
	struct scope *scope = &state->scopes[--state->nr_scopes];

	// XXX: no need to delete at end of function scope
	if (state->nr_scopes == 0) {
		free(scope->vars);
		return;
	}

	for (size_t i = 0; i < scope->nr_vars; i++) {
		struct ain_function *f = &state->ain->functions[state->func_no];
		struct ain_variable *v = &f->vars[scope->vars[i]];
		if (v->type.data == AIN_ARRAY || v->type.data == AIN_REF_ARRAY) {
			// TODO
		} else if (v->type.data == AIN_STRUCT || v->type.data == AIN_REF_STRUCT) {
			// .LOCALDELETE var
			// TODO: use SH_LOCALDELETE when available
			write_instruction0(state, PUSHLOCALPAGE);
			write_instruction1(state, PUSH, scope->vars[i]);
			write_instruction0(state, DUP2);
			write_instruction0(state, REF);
			write_instruction0(state, DELETE);
			write_instruction1(state, PUSH, -1);
			write_instruction0(state, ASSIGN);
			write_instruction0(state, POP);
		}
	}
	free(scope->vars);
}

static void scope_add_variable(struct compiler_state *state, int var_no)
{
	struct scope *scope = &state->scopes[state->nr_scopes-1];
	scope->vars = xrealloc_array(scope->vars, scope->nr_vars, scope->nr_vars+1, sizeof(int));
	scope->vars[scope->nr_vars++] = var_no;
}

static void start_function(struct compiler_state *state)
{
	state->nr_labels = 0;
	state->nr_gotos = 0;
	state->labels = NULL;
	state->gotos = NULL;
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

static uint32_t flo2int(float f)
{
	union { uint32_t i; float f; } v = { .f = f};
	return v.i;
}

static bool is_integer_type(enum ain_data_type type)
{
	switch (type) {
	case AIN_INT:
	case AIN_REF_INT:
	case AIN_BOOL:
	case AIN_REF_BOOL:
	case AIN_ENUM:
	case AIN_REF_ENUM:
		return true;
	default:
		return false;
	}
}

static void write_instruction_for_op(struct compiler_state *state, enum jaf_operator op, enum ain_data_type lhs_type, enum ain_data_type rhs_type)
{
	if (lhs_type == AIN_FLOAT || lhs_type == AIN_REF_FLOAT) {
		switch (op) {
		case JAF_MULTIPLY:      write_instruction0(state, F_MUL); break;
		case JAF_DIVIDE:        write_instruction0(state, F_DIV); break;
		case JAF_PLUS:          write_instruction0(state, F_ADD); break;
		case JAF_MINUS:         write_instruction0(state, F_SUB); break;
		case JAF_LT:            write_instruction0(state, F_LT); break;
		case JAF_GT:            write_instruction0(state, F_GT); break;
		case JAF_LTE:           write_instruction0(state, F_LTE); break;
		case JAF_GTE:           write_instruction0(state, F_GTE); break;
		case JAF_EQ:            write_instruction0(state, F_EQUALE); break;
		case JAF_NEQ:           write_instruction0(state, F_NOTE); break;
		case JAF_ASSIGN:        write_instruction0(state, F_ASSIGN); break;
		case JAF_MUL_ASSIGN:    write_instruction0(state, F_MULA); break;
		case JAF_DIV_ASSIGN:    write_instruction0(state, F_DIVA); break;
		case JAF_ADD_ASSIGN:    write_instruction0(state, F_PLUSA); break;
		case JAF_SUB_ASSIGN:    write_instruction0(state, F_MINUSA); break;
		default:                _COMPILER_ERROR(NULL, -1, "Invalid floating point operator");
		}
	} else if (is_integer_type(lhs_type)) {
		switch (op) {
		case JAF_MULTIPLY:      write_instruction0(state, MUL); break;
		case JAF_DIVIDE:        write_instruction0(state, DIV); break;
		case JAF_REMAINDER:     write_instruction0(state, MOD); break;
		case JAF_PLUS:          write_instruction0(state, ADD); break;
		case JAF_MINUS:         write_instruction0(state, SUB); break;
		case JAF_LSHIFT:        write_instruction0(state, LSHIFT); break;
		case JAF_RSHIFT:        write_instruction0(state, RSHIFT); break;
		case JAF_LT:            write_instruction0(state, LT); break;
		case JAF_GT:            write_instruction0(state, GT); break;
		case JAF_LTE:           write_instruction0(state, LTE); break;
		case JAF_GTE:           write_instruction0(state, GTE); break;
		case JAF_EQ:            write_instruction0(state, EQUALE); break;
		case JAF_NEQ:           write_instruction0(state, NOTE); break;
		case JAF_BIT_AND:       write_instruction0(state, AND); break;
		case JAF_BIT_XOR:       write_instruction0(state, XOR); break;
		case JAF_BIT_IOR:       write_instruction0(state, OR); break;
		//case JAF_LOG_AND:       write_instruction0(state, AND); break;
		//case JAF_LOG_OR:        write_instruction0(state, OR); break;
		case JAF_ASSIGN:        write_instruction0(state, ASSIGN); break;
		case JAF_MUL_ASSIGN:    write_instruction0(state, MULA); break;
		case JAF_DIV_ASSIGN:    write_instruction0(state, DIVA); break;
		case JAF_MOD_ASSIGN:    write_instruction0(state, MODA); break;
		case JAF_ADD_ASSIGN:    write_instruction0(state, PLUSA); break;
		case JAF_SUB_ASSIGN:    write_instruction0(state, MINUSA); break;
		case JAF_LSHIFT_ASSIGN: write_instruction0(state, LSHIFTA); break;
		case JAF_RSHIFT_ASSIGN: write_instruction0(state, RSHIFTA); break;
		case JAF_AND_ASSIGN:    write_instruction0(state, ANDA); break;
		case JAF_XOR_ASSIGN:    write_instruction0(state, XORA); break;
		case JAF_OR_ASSIGN:     write_instruction0(state, ORA); break;
		default:                _COMPILER_ERROR(NULL, -1, "Invalid integer operator");
		}
	} else if (lhs_type == AIN_STRING || lhs_type == AIN_REF_STRING) {
		switch (op) {
		case JAF_PLUS:       write_instruction0(state, S_ADD); break;
		case JAF_LT:         write_instruction0(state, S_LT); break;
		case JAF_GT:         write_instruction0(state, S_GT); break;
		case JAF_LTE:        write_instruction0(state, S_LTE); break;
		case JAF_GTE:        write_instruction0(state, S_GTE); break;
		case JAF_EQ:         write_instruction0(state, S_EQUALE); break;
		case JAF_NEQ:        write_instruction0(state, S_NOTE); break;
		case JAF_ASSIGN:     write_instruction0(state, S_ASSIGN); break;
		case JAF_REMAINDER:
			switch (rhs_type) {
			case AIN_INT:
			case AIN_ENUM:
				write_instruction1(state, S_MOD, 2); break;
			case AIN_FLOAT:  write_instruction1(state, S_MOD, 3); break;
			case AIN_STRING: write_instruction1(state, S_MOD, 4); break;
			default:         _COMPILER_ERROR(NULL, -1, "Invalid type for string formatting");
			}
			break;
		default:             _COMPILER_ERROR(NULL, -1, "Invalid string operator");
		}
	} else {
		_COMPILER_ERROR(NULL, -1, "Invalid operator type");
	}
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

static struct ain_variable *get_identifier_variable(struct compiler_state *state, enum ain_variable_type type, int var_no)
{
	if (type == AIN_VAR_LOCAL)
		return &state->ain->functions[state->func_no].vars[var_no];
	if (type == AIN_VAR_GLOBAL)
		return &state->ain->globals[var_no];
	_COMPILER_ERROR(NULL, -1, "Invalid variable type");
}

static void compile_identifier_ref(struct compiler_state *state, enum ain_variable_type type, int var_no)
{
	if (type == AIN_VAR_LOCAL) {
		write_instruction0(state, PUSHLOCALPAGE);
	} else if (type == AIN_VAR_GLOBAL) {
		write_instruction0(state, PUSHGLOBALPAGE);
	} else {
		_COMPILER_ERROR(NULL, -1, "Invalid variable type");
	}
	write_instruction1(state, PUSH, var_no);
}

static void compile_lvalue_after(struct compiler_state *state, enum ain_data_type type)
{
	switch (type) {
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
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
		write_instruction0(state, REF);
		break;
	default:
		break;
	}
}

static void compile_lvalue(struct compiler_state *state, struct jaf_expression *expr);
static void compile_new_lvalue(struct compiler_state *state, struct jaf_expression *expr);

static void compile_variable(struct compiler_state *state, struct jaf_expression *expr)
{
	if (expr->type == JAF_EXP_IDENTIFIER) {
		compile_identifier_ref(state, expr->ident.var_type, expr->ident.var_no);
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

static void compile_lvalue(struct compiler_state *state, struct jaf_expression *expr)
{
	if (expr->type == JAF_EXP_IDENTIFIER) {
		struct ain_variable *v = get_identifier_variable(state, expr->ident.var_type, expr->ident.var_no);
		compile_identifier_ref(state, expr->ident.var_type, expr->ident.var_no);
		compile_lvalue_after(state, v->type.data);
	} else if (expr->type == JAF_EXP_MEMBER) {
		compile_lvalue(state, expr->member.struc);
		write_instruction1(state, PUSH, expr->member.member_no);
		compile_lvalue_after(state, expr->valuetype.data);
	} else if (expr->type == JAF_EXP_SUBSCRIPT) {
		compile_lvalue(state, expr->subscript.expr);  // page
		compile_expression(state, expr->subscript.index); // page-index
		compile_lvalue_after(state, expr->valuetype.data);
	} else if (expr->type == JAF_EXP_THIS) {
		compile_expression(state, expr);
	} else if (expr->type == JAF_EXP_NEW) {
		compile_new_lvalue(state, expr);
	} else {
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
	default:
		_COMPILER_ERROR(NULL, -1, "Unsupported type");
	}
}

static void compile_constant_identifier(struct compiler_state *state, struct jaf_expression *expr)
{
	switch (expr->ident.val.data_type) {
	case AIN_INT:
		compile_int(state, expr->ident.val.int_value);
		break;
	case AIN_FLOAT:
		compile_float(state, expr->ident.val.float_value);
		break;
	case AIN_STRING:
		compile_string(state, expr->ident.val.string_value);
		break;
	default:
		COMPILER_ERROR(expr, "Unhandled constant type");
	}
}

static void compile_identifier(struct compiler_state *state, struct jaf_expression *expr)
{
	if (expr->ident.is_const) {
		compile_constant_identifier(state, expr);
		return;
	}
	struct ain_variable *var = get_identifier_variable(state, expr->ident.var_type, expr->ident.var_no);
	compile_identifier_ref(state, expr->ident.var_type, expr->ident.var_no);
	compile_dereference(state, &var->type);
}

/*
 * Pop a value of the given type off the stack.
 */
static void compile_pop(struct compiler_state *state, enum ain_data_type type)
{
	switch (type) {
	case AIN_VOID:
		break;
	case AIN_INT:
	case AIN_FLOAT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
	case AIN_REF_STRUCT: // ...
		write_instruction0(state, POP);
		break;
	case AIN_STRING:
	case AIN_REF_STRING:
		write_instruction0(state, AIN_VERSION_GTE(state->ain, 11, 0) ? DELETE : S_POP);
		break;
	default:
		_COMPILER_ERROR(NULL, -1, "Unsupported type");
	}
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
		write_instruction0(state, INC);
		break;
	case JAF_PRE_DEC:
		compile_lvalue(state, expr->expr);
		write_instruction0(state, DEC);
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
		write_instruction_for_op(state, expr->op, expr->lhs->valuetype.data, expr->rhs->valuetype.data);
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
		// FIXME: I don't think this works for assigning to ref types
		compile_lvalue(state, expr->lhs);
		compile_expression(state, expr->rhs);
		write_instruction_for_op(state, expr->op, expr->valuetype.data, expr->rhs->valuetype.data);
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

static bool ain_ref_type(enum ain_data_type type)
{
	switch (type) {
	case AIN_REF_TYPE:
	case AIN_WRAP:
		return true;
	default:
		return false;
	}
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

static void compile_argument(struct compiler_state *state, struct jaf_expression *arg, enum ain_data_type type)
{
	if (ain_ref_type(type)) {
		compile_reference_argument(state, arg);
	} else {
		compile_expression(state, arg);
	}
}

static void compile_function_arguments(struct compiler_state *state, struct jaf_argument_list *args, int func_no)
{
	for (size_t i = 0; i < args->nr_items; i++) {
		struct ain_function *f = &state->ain->functions[func_no];
		compile_argument(state, args->items[i], f->vars[args->var_nos[i]].type.data);
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
		if (ain_ref_type(type)) {
			compile_reference_argument(state, expr->call.args->items[i]);
			write_instruction0(state, DUP2_X1);
			write_instruction0(state, POP);
			write_instruction0(state, POP);
		} else {
			compile_expression(state, expr->call.args->items[i]);
			write_instruction0(state, SWAP);
		}
	}
	write_instruction1(state, PUSH, expr->call.fun->valuetype.struc);
	write_instruction0(state, CALLFUNC2);
}

static void compile_funcall(struct compiler_state *state, struct jaf_expression *expr)
{
	if (expr->call.fun->valuetype.data == AIN_FUNC_TYPE) {
		jaf_compile_functype_call(state, expr);
		return;
	}
	compile_function_arguments(state, expr->call.args, expr->call.func_no);
	write_instruction1(state, CALLFUNC, expr->call.func_no);
}

static void compile_method_call(struct compiler_state *state, struct jaf_expression *expr)
{
	assert(expr->call.fun->type == JAF_EXP_MEMBER);
	compile_lvalue(state, expr->call.fun->member.struc);

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

static void compile_builtin_call(possibly_unused struct compiler_state *state, struct jaf_expression *expr)
{
	// TODO: pre-v11 builtins (instruction based)
	JAF_ERROR(expr, "built-in methods not supported");
}

static void compile_new_lvalue(struct compiler_state *state, struct jaf_expression *expr)
{
	// FIXME: the scope of this variable is the current statement, not the block scope!
	scope_add_variable(state, expr->new.var_no);

	// delete dummy variable
	write_instruction0(state, PUSHLOCALPAGE);
	write_instruction1(state, PUSH, expr->new.var_no);
	write_instruction0(state, REF);
	write_instruction0(state, DELETE);

	// prepare for assign to dummy variable
	write_instruction0(state, PUSHLOCALPAGE);
	write_instruction1(state, PUSH, expr->new.var_no);

	// call constructor (via NEW)
	compile_function_arguments(state, expr->new.args, expr->new.func_no);
	compile_lock_peek(state);
	if (AIN_VERSION_GTE(state->ain, 11, 0)) {
		write_instruction2(state, NEW, expr->valuetype.struc, expr->new.func_no);
	} else {
		write_instruction1(state, NEW, expr->valuetype.struc);
	}

	// assign to dummy variable
	write_instruction0(state, ASSIGN);
	compile_unlock_peek(state);
}

static void compile_new(struct compiler_state *state, struct jaf_expression *expr)
{
	compile_new_lvalue(state, expr);
	write_instruction0(state, A_REF);
}

static void compile_cast(struct compiler_state *state, struct jaf_expression *expr)
{
	enum ain_data_type src_type = expr->cast.expr->valuetype.data;
	enum ain_data_type dst_type = expr->valuetype.data;
	compile_expression(state, expr->cast.expr);

	if (src_type == dst_type)
		return;
	if (src_type == AIN_INT) {
		if (dst_type == AIN_FLOAT) {
			write_instruction0(state, ITOF);
		} else if (dst_type == AIN_STRING) {
			write_instruction0(state, I_STRING);
		} else {
			goto invalid_cast;
		}
	} else if (src_type == AIN_FLOAT) {
		if (dst_type == AIN_INT) {
			write_instruction0(state, FTOI);
		} else if (dst_type == AIN_STRING) {
			write_instruction0(state, FTOS);
		} else {
			goto invalid_cast;
		}
	} else if (src_type == AIN_STRING) {
		if (dst_type == AIN_INT) {
			write_instruction0(state, STOI);
		} else {
			goto invalid_cast;
		}
	} else {
		goto invalid_cast;
	}
	return;
invalid_cast:
	JAF_ERROR(expr, "Unsupported cast: %s to %s",
		  ain_strtype(state->ain, src_type, -1),
		  jaf_type_to_string(expr->cast.type));
}

static void compile_member(struct compiler_state *state, struct jaf_expression *expr)
{
	compile_lvalue(state, expr->member.struc);
	write_instruction1(state, PUSH, expr->member.member_no);
	compile_dereference(state, &expr->valuetype);
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
	compile_dereference(state, &expr->valuetype);
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
	case JAF_EXP_BUILTIN_CALL:
		compile_builtin_call(state, expr);
		break;
	case JAF_EXP_NEW:
		compile_new(state, expr);
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
	}
}

static void compile_nullexpr(struct compiler_state *state, enum ain_data_type type)
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
		write_instruction1(state, PUSH, -1);
		break;
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
		write_instruction1(state, PUSH, -1);
		write_instruction1(state, PUSH, 0);
		break;
	default:
		_COMPILER_ERROR(NULL, -1, "Unsupported type: %s", ain_strtype(state->ain, type, -1));
	}
}

static void compile_vardecl(struct compiler_state *state, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	if (decl->type->qualifiers & JAF_QUAL_CONST) {
		return;
	}
	switch (decl->valuetype.data) {
	case AIN_VOID:
		COMPILER_ERROR(item, "void variable declaration");
	case AIN_INT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_FUNC_TYPE:
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, decl->var_no);
		if (decl->init)
			compile_expression(state, decl->init);
		else
			write_instruction1(state, PUSH, 0);
		write_instruction0(state, decl->valuetype.data == AIN_LONG_INT ? LI_ASSIGN : ASSIGN);
		write_instruction0(state, POP);
		break;
	case AIN_FLOAT:
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, decl->var_no);
		if (decl->init)
			compile_expression(state, decl->init);
		else
			write_instruction1(state, F_PUSH, 0);
		write_instruction0(state, F_ASSIGN);
		write_instruction0(state, POP);
		break;
	case AIN_STRING:
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			write_instruction0(state, PUSHLOCALPAGE);
			write_instruction1(state, PUSH, decl->var_no);
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
		} else {
			write_instruction0(state, PUSHLOCALPAGE);
			write_instruction1(state, PUSH, decl->var_no);
			write_instruction0(state, REF);
			if (decl->init) {
				compile_expression(state, decl->init);
			} else {
				write_instruction1(state, S_PUSH, 0);
			}
			write_instruction0(state, S_ASSIGN);
			write_instruction0(state, state->ain->version >= 11 ? DELETE : S_POP);
		}
		break;
	case AIN_STRUCT:
		write_instruction1(state, SH_LOCALDELETE, decl->var_no); // FIXME: use verbose version
		write_instruction2(state, SH_LOCALCREATE, decl->var_no, decl->valuetype.struc);
		break;
	case AIN_REF_INT:
	case AIN_REF_BOOL:
	case AIN_REF_FLOAT:
	case AIN_REF_LONG_INT:
	case AIN_REF_FUNC_TYPE:
		compile_lock_peek(state);
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, decl->var_no);
		write_instruction0(state, DUP2);
		write_instruction0(state, REF);
		write_instruction0(state, DELETE);
		if (decl->init) {
			compile_lvalue(state, decl->init);
		} else {
			write_instruction1(state, PUSH, -1);
			write_instruction1(state, PUSH, 0);
		}
		write_instruction0(state, R_ASSIGN);
		write_instruction0(state, POP);
		write_instruction0(state, SP_INC);
		compile_unlock_peek(state);
		break;
	case AIN_REF_STRING:
		compile_lock_peek(state);
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, decl->var_no);
		write_instruction0(state, DUP2);
		write_instruction0(state, REF);
		write_instruction0(state, DELETE);
		if (decl->init) {
			compile_lvalue(state, decl->init);
		} else {
			write_instruction1(state, PUSH, -1);
		}
		write_instruction0(state, ASSIGN);
		write_instruction0(state, SP_INC);
		compile_unlock_peek(state);
		break;
	case AIN_REF_STRUCT:
		compile_lock_peek(state);
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, decl->var_no);
		write_instruction0(state, DUP2);
		write_instruction0(state, REF);
		write_instruction0(state, DELETE);
		if (decl->init) {
			write_instruction0(state, DUP2);
			compile_lvalue(state, decl->init);
			write_instruction0(state, ASSIGN);
			write_instruction0(state, DUP_X2);
			write_instruction0(state, POP);
			write_instruction0(state, SP_INC);
			write_instruction0(state, POP);
		} else {
			write_instruction1(state, PUSH, -1);
			write_instruction0(state, ASSIGN);
			write_instruction0(state, POP);
		}
		compile_unlock_peek(state);
		break;
	case AIN_ARRAY:
	case AIN_ARRAY_TYPE:
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			write_instruction0(state, PUSHLOCALPAGE);
			write_instruction1(state, PUSH, decl->var_no);
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
					write_instruction0(state, PUSHLOCALPAGE);
					write_instruction1(state, PUSH, decl->var_no);
					write_instruction0(state, REF);
					write_instruction1(state, PUSH, decl->array_dims[0]->i);
					write_CALLHLL(state, "Array", "Alloc", 1); // ???
				}
			}
		} else if (AIN_VERSION_GTE(state->ain, 11, 0)) {
			write_instruction0(state, PUSHLOCALPAGE);
			write_instruction1(state, PUSH, decl->var_no);
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
			write_instruction0(state, PUSHLOCALPAGE);
			write_instruction1(state, PUSH, decl->var_no);
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
	default:
		COMPILER_ERROR(item, "Unsupported variable type: %d", decl->valuetype.data);
	}

	switch (decl->valuetype.data) {
	case AIN_REF_TYPE:
		scope_add_variable(state, decl->var_no);
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
	compile_expression(state, test);
	addr[1] = state->out.index + 2; // address of address of loop end
	write_instruction1(state, IFZ, 0);
	addr[2] = state->out.index + 2; // address of address of loop body
	write_instruction1(state, JUMP, 0);
	// loop increment
	start_loop(state);
	compile_expression(state, after);
	compile_pop(state, after->valuetype.data);
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
	write_instruction1(state, MSG, no);
	if (item->msg.func)
		write_instruction1(state, CALLFUNC, item->msg.func_no);
}

static void compile_rassign(struct compiler_state *state, struct jaf_block_item *item)
{
	// delete previous reference
	compile_lock_peek(state);
	compile_variable(state, item->rassign.lhs);
	write_instruction0(state, DUP2);
	write_instruction0(state, REF);
	write_instruction0(state, DELETE);

	switch (item->rassign.lhs->valuetype.data) {
	case AIN_REF_INT:
	case AIN_REF_BOOL:
	case AIN_REF_FLOAT:
	case AIN_REF_LONG_INT:
	case AIN_REF_FUNC_TYPE:
		compile_lvalue(state, item->rassign.rhs);
		write_instruction0(state, R_ASSIGN);
		write_instruction0(state, POP);
		write_instruction0(state, SP_INC);
		break;
	case AIN_REF_STRUCT:
		// XXX: Reference assignment to a 'ref struct' variable is a bit weird.
		//      Unlike other types, the RHS can be an rvalue.
		switch (item->rassign.rhs->type) {
		case JAF_EXP_IDENTIFIER:
		case JAF_EXP_MEMBER:
		case JAF_EXP_SUBSCRIPT:
			compile_lvalue(state, item->rassign.rhs);
			break;
		default:
			compile_lvalue(state, item->rassign.rhs);
			//compile_expression(state, item->rassign.rhs);
			break;
		}
		write_instruction0(state, ASSIGN);
		write_instruction0(state, SP_INC);
		break;
	case AIN_REF_STRING:
	case AIN_REF_ARRAY_TYPE:
	case AIN_REF_ARRAY:
		compile_lvalue(state, item->rassign.rhs);
		write_instruction0(state, ASSIGN);
		write_instruction0(state, SP_INC);
		break;
	default:
		COMPILER_ERROR(item, "Invalid LHS in reference assignment");
	}
	compile_unlock_peek(state);
}

static void compile_statement(struct compiler_state *state, struct jaf_block_item *item)
{
	if (item->is_scope) {
		start_scope(state);
	}

	switch (item->kind) {
	case JAF_DECL_VAR:
		compile_vardecl(state, item);
		break;
	case JAF_DECL_FUNCTYPE:
		JAF_ERROR(item, "Function types must be declared at top-level");
	case JAF_DECL_FUN:
		JAF_ERROR(item, "Functions must be defined at top-level");
	case JAF_DECL_STRUCT:
		JAF_ERROR(item, "Structs must be defined at top-level");
	case JAF_STMT_NULL:
		break;
	case JAF_STMT_LABELED:
		compile_label(state, item);
		break;
	case JAF_STMT_COMPOUND:
		compile_block(state, item->block);
		break;
	case JAF_STMT_EXPRESSION:
		compile_expression(state, item->expr);
		compile_pop(state, item->expr->valuetype.data);
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
		if (item->expr)
			compile_expression(state, item->expr);
		write_instruction0(state, RETURN);
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

static void compile_function(struct compiler_state *state, struct jaf_fundecl *decl)
{
	assert(decl->func_no >= 0 && decl->func_no < state->ain->nr_functions);

	start_function(state);
	state->func_no = decl->func_no;
	state->super_no = decl->super_no;

	write_instruction1(state, FUNC, decl->func_no);
	state->ain->functions[decl->func_no].address = state->out.index;
	compile_block(state, decl->body);
	compile_nullexpr(state, state->ain->functions[state->func_no].return_type.data);
	write_instruction0(state, RETURN);
	write_instruction1(state, ENDFUNC, decl->func_no);

	end_function(state);
}

static void compile_declaration(struct compiler_state *state, struct jaf_block_item *decl)
{
	if (decl->kind == JAF_DECL_FUN) {
		compile_function(state, &decl->fun);
		return;
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
		}
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
	jaf_compile(out, toplevel);
	jaf_free_block(toplevel);
}
