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

struct compiler_state {
	struct ain *ain;
	struct buffer out;
	int func_no;
	size_t nr_loops;
	struct loop_state *loops;
};

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

static int get_string_no(struct compiler_state *state, struct string *s)
{
	char *u = encode_text(s->text);
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
	write_opcode(state, opcode);
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

static uint32_t flo2int(float f)
{
	union { uint32_t i; float f; } v = { .f = f};
	return v.i;
}

static enum opcode jaf_op_to_opcode(enum jaf_operator op, enum ain_data_type type)
{
	if (type == AIN_FLOAT || type == AIN_REF_FLOAT) {
		switch (op) {
		case JAF_MULTIPLY:      return F_MUL;
		case JAF_DIVIDE:        return F_DIV;
		case JAF_PLUS:          return F_ADD;
		case JAF_MINUS:         return F_SUB;
		case JAF_LT:            return F_LT;
		case JAF_GT:            return F_GT;
		case JAF_LTE:           return F_LTE;
		case JAF_GTE:           return F_GTE;
		case JAF_EQ:            return F_EQUALE;
		case JAF_NEQ:           return F_NOTE;
		case JAF_ASSIGN:        return F_ASSIGN;
		case JAF_MUL_ASSIGN:    return F_MULA;
		case JAF_DIV_ASSIGN:    return F_DIVA;
		case JAF_ADD_ASSIGN:    return F_PLUSA;
		case JAF_SUB_ASSIGN:    return F_MINUSA;
		case JAF_REF_ASSIGN:    // TODO
		default:                ERROR("Invalid floating point operator");
		}
	} else if (type == AIN_INT || type == AIN_REF_INT) {
		switch (op) {
		case JAF_MULTIPLY:      return MUL;
		case JAF_DIVIDE:        return DIV;
		case JAF_REMAINDER:     return MOD;
		case JAF_PLUS:          return ADD;
		case JAF_MINUS:         return SUB;
		case JAF_LSHIFT:        return LSHIFT;
		case JAF_RSHIFT:        return RSHIFT;
		case JAF_LT:            return LT;
		case JAF_GT:            return GT;
		case JAF_LTE:           return LTE;
		case JAF_GTE:           return GTE;
		case JAF_EQ:            return EQUALE;
		case JAF_NEQ:           return NOTE;
		case JAF_BIT_AND:       return AND;
		case JAF_BIT_XOR:       return XOR;
		case JAF_BIT_IOR:       return OR;
		//case JAF_LOG_AND:       return AND;
		//case JAF_LOG_OR:        return OR;
		case JAF_ASSIGN:        return ASSIGN;
		case JAF_MUL_ASSIGN:    return MULA;
		case JAF_DIV_ASSIGN:    return DIVA;
		case JAF_MOD_ASSIGN:    return MODA;
		case JAF_ADD_ASSIGN:    return PLUSA;
		case JAF_SUB_ASSIGN:    return MINUSA;
		case JAF_LSHIFT_ASSIGN: return LSHIFTA;
		case JAF_RSHIFT_ASSIGN: return RSHIFTA;
		case JAF_AND_ASSIGN:    return ANDA;
		case JAF_XOR_ASSIGN:    return XORA;
		case JAF_OR_ASSIGN:     return ORA;
		case JAF_REF_ASSIGN:    // TODO
		default:                ERROR("Invalid integer operator");
		}
	} else if (type == AIN_STRING || type == AIN_REF_STRING) {
		switch (op) {
		case JAF_PLUS:       return S_ADD;
		case JAF_LT:         return S_LT;
		case JAF_GT:         return S_GT;
		case JAF_LTE:        return S_LTE;
		case JAF_GTE:        return S_GTE;
		case JAF_EQ:         return S_EQUALE;
		case JAF_NEQ:        return S_NOTE;
		case JAF_ASSIGN:     return S_ASSIGN;
		default:             ERROR("Invalid string operator");
		}
	} else {
		ERROR("Invalid operator type");
	}
}

static void compile_block(struct compiler_state *state, struct jaf_block *block);
static void compile_statement(struct compiler_state *state, struct jaf_block_item *item);
static void compile_expression(struct compiler_state *state, struct jaf_expression *expr);

static struct ain_variable *get_identifier_variable(struct compiler_state *state, enum ain_variable_type type, int var_no)
{
	if (type == AIN_VAR_LOCAL)
		return &state->ain->functions[state->func_no].vars[var_no];
	if (type == AIN_VAR_GLOBAL)
		return &state->ain->globals[var_no];
	ERROR("Invalid variable type");
}

static void compile_identifier_ref(struct compiler_state *state, enum ain_variable_type type, int var_no)
{
	if (type == AIN_VAR_LOCAL) {
		write_instruction0(state, PUSHLOCALPAGE);
	} else if (type == AIN_VAR_GLOBAL) {
		write_instruction0(state, PUSHGLOBALPAGE);
	} else {
		ERROR("Invalid variable type");
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
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE:
	case AIN_STRUCT:
	case AIN_REF_STRUCT:
		write_instruction0(state, REF);
		break;
	default:
		break;
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
	} else {
		ERROR("Invalid lvalue");
	}
}

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
		write_instruction0(state, state->ain->version >= 11 ? A_REF : S_REF);
		break;
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE:
		write_instruction0(state, REF);
		write_instruction0(state, A_REF);
		break;
	case AIN_STRUCT:
	case AIN_REF_STRUCT:
		write_instruction1(state, SR_REF, type->struc);
		break;
	default:
		ERROR("Unsupported type");
	}
}

static void compile_identifier(struct compiler_state *state, struct jaf_expression *expr)
{
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
		write_instruction0(state, POP);
		break;
	case AIN_STRING:
	case AIN_REF_STRING:
		write_instruction0(state, state->ain->version >= 11 ? DELETE : S_POP);
		break;
	default:
		ERROR("Unsupported type");
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
		write_instruction0(state, DUP2);
		write_instruction0(state, REF);
		write_instruction0(state, DUP_X2);
		write_instruction0(state, POP);
		write_instruction0(state, INC);
		break;
	case JAF_POST_DEC:
		compile_lvalue(state, expr->expr);
		write_instruction0(state, DUP2);
		write_instruction0(state, REF);
		write_instruction0(state, DUP_X2);
		write_instruction0(state, POP);
		write_instruction0(state, DEC);
		break;
	default:
		ERROR("Invalid unary operator");
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
		write_instruction0(state, jaf_op_to_opcode(expr->op, expr->lhs->valuetype.data));
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
		compile_lvalue(state, expr->lhs);
		compile_expression(state, expr->rhs);
		write_instruction0(state, jaf_op_to_opcode(expr->op, expr->valuetype.data));
		break;
	case JAF_REF_ASSIGN:
	default:
		ERROR("Invalid binary operator");
	}
}

static void compile_ternary(struct compiler_state *state, struct jaf_expression *expr)
{
	ERROR("Ternary operator not supported");
}

static bool ain_ref_type(enum ain_data_type type)
{
	switch (type) {
	case AIN_REF_TYPE:
		return true;
	default:
		return false;
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
			compile_lvalue(state, expr->call.args->items[i]);
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

	for (size_t i = 0; i < expr->call.args->nr_items; i++) {
		struct ain_function *f = &state->ain->functions[expr->call.func_no];
		if (ain_ref_type(f->vars[expr->call.args->var_nos[i]].type.data)) {
			compile_lvalue(state, expr->call.args->items[i]);
		} else {
			compile_expression(state, expr->call.args->items[i]);
		}
	}
	write_instruction1(state, CALLFUNC, expr->call.func_no);
}

static void compile_syscall(struct compiler_state *state, struct jaf_expression *expr)
{
	unsigned nr_args = expr->call.args ? expr->call.args->nr_items : 0;
	for (unsigned i = 0; i < nr_args; i++) {
		if (ain_ref_type(syscalls[expr->call.func_no].argtypes[i])) {
			compile_lvalue(state, expr->call.args->items[i]);
		} else {
			compile_expression(state, expr->call.args->items[i]);
		}
	}

	if (state->ain->version >= 11) {
		ERROR("Syscalls not supported");
	} else {
		write_instruction1(state, CALLSYS, expr->call.func_no);
	}
}

static void compile_cast(struct compiler_state *state, struct jaf_expression *expr)
{
	enum ain_data_type src_type = expr->cast.expr->valuetype.data;
	enum ain_data_type dst_type = jaf_to_ain_data_type(expr->cast.type, 0);
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
	}
	return;
invalid_cast:
	ERROR("Unsupported cast: %s to %s", ain_strtype(state->ain, src_type, -1), jaf_typestr(expr->cast.type));
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
		write_instruction1(state, PUSH, expr->i);
		break;
	case JAF_EXP_FLOAT:
		write_instruction1(state, F_PUSH, flo2int(expr->f));
		break;
	case JAF_EXP_STRING:
		write_instruction1(state, S_PUSH, get_string_no(state, expr->s));
		break;
	case JAF_EXP_IDENTIFIER:
		compile_identifier(state, expr);
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
		ERROR("Unresolved character constant"); // should have been simplified to int
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
		ERROR("Unsupported type: %s", ain_strtype(state->ain, type, -1));
	}
}

static void compile_vardecl(struct compiler_state *state, struct jaf_vardecl *decl)
{
	enum ain_data_type type = jaf_to_ain_data_type(decl->type->type, decl->type->qualifiers);
	switch (type) {
	case AIN_VOID:
		ERROR("void variable declaration");
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
		write_instruction0(state, type == AIN_LONG_INT ? LI_ASSIGN : ASSIGN);
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
		write_instruction0(state, PUSHLOCALPAGE);
		write_instruction1(state, PUSH, decl->var_no);
		write_instruction0(state, REF);
		if (decl->init)
			compile_expression(state, decl->init);
		else
			write_instruction1(state, S_PUSH, 0);
		write_instruction0(state, S_ASSIGN);
		write_instruction0(state, state->ain->version >= 11 ? DELETE : S_POP);
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
		if (state->ain->version < 6) {
			write_instruction1(state, CALLSYS, SYS_LOCK_PEEK);
			write_instruction0(state, POP);
		}
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
		if (state->ain->version < 6) {
			write_instruction1(state, CALLSYS, SYS_UNLOCK_PEEK);
			write_instruction0(state, POP);
		}
		break;
	case AIN_REF_STRING:
		if (state->ain->version < 6) {
			write_instruction1(state, CALLSYS, SYS_LOCK_PEEK);
			write_instruction0(state, POP);
		}
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
		if (state->ain->version < 6) {
			write_instruction1(state, CALLSYS, SYS_UNLOCK_PEEK);
			write_instruction0(state, POP);
		}
		break;
	case AIN_REF_STRUCT:
		if (state->ain->version < 6) {
			write_instruction1(state, CALLSYS, SYS_LOCK_PEEK);
			write_instruction0(state, POP);
		}
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
		if (state->ain->version < 6) {
			write_instruction1(state, CALLSYS, SYS_UNLOCK_PEEK);
			write_instruction0(state, POP);
		}
		break;
	case AIN_ARRAY_TYPE:
		if (state->ain->version >= 11)
			ERROR("Arrays not supported on ain v11+");
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
		break;
	default:
		ERROR("Unsupported variable type: %d", type);
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
	write_instruction1(state, JUMP, addr[0]);
	// loop body
	buffer_write_int32_at(&state->out, addr[2], state->out.index);
	compile_statement(state, body);
	write_instruction1(state, JUMP, state->loops[state->nr_loops-1].loop_addr);
	// loop end
	buffer_write_int32_at(&state->out, addr[1], state->out.index);
	end_loop(state);
}

static void compile_break(struct compiler_state *state)
{
	if (state->nr_loops == 0)
		ERROR("break outside of loop");
	struct loop_state *loop = &state->loops[state->nr_loops-1];
	loop->breaks = xrealloc_array(loop->breaks, loop->nr_breaks, loop->nr_breaks+1, sizeof(uint32_t));
	loop->breaks[loop->nr_breaks++] = state->out.index + 2;
	write_instruction1(state, JUMP, 0);
}

static void compile_message(struct compiler_state *state, struct jaf_block_item *item)
{
	int no = ain_add_message(state->ain, item->msg.text->text);
	write_instruction1(state, MSG, no);
	if (item->msg.func)
		write_instruction1(state, CALLFUNC, item->msg.func_no);
}

static void compile_statement(struct compiler_state *state, struct jaf_block_item *item)
{
	switch (item->kind) {
	case JAF_DECL_VAR:
		compile_vardecl(state, &item->var);
		break;
	case JAF_DECL_FUNCTYPE:
		ERROR("Function types must be declared at top-level");
	case JAF_DECL_FUN:
		ERROR("Functions must be defined at top-level");
	case JAF_DECL_STRUCT:
		ERROR("Structs must be defined at top-level");
	case JAF_STMT_LABELED:
		ERROR("Labels not supported");
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
		ERROR("switch not supported");
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
		ERROR("goto not supported");
	case JAF_STMT_CONTINUE:
		if (state->nr_loops == 0)
			ERROR("continue outside of loop");
		write_instruction1(state, JUMP, state->loops[state->nr_loops-1].loop_addr);
		break;
	case JAF_STMT_BREAK:
		compile_break(state);
		break;
	case JAF_STMT_RETURN:
		if (item->expr)
			compile_expression(state, item->expr);
		write_instruction0(state, RETURN);
		break;
	case JAF_STMT_CASE:
		ERROR("switch not supported");
		break;
	case JAF_STMT_DEFAULT:
		ERROR("switch not supported");
		break;
	case JAF_STMT_MESSAGE:
		compile_message(state, item);
		break;
	case JAF_EOF:
		write_instruction1(state, _EOF, item->file_no);
		break;
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
	state->func_no = decl->func_no;
	write_instruction1(state, FUNC, decl->func_no);
	state->ain->functions[decl->func_no].address = state->out.index;
	compile_block(state, decl->body);
	compile_nullexpr(state, state->ain->functions[state->func_no].return_type.data);
	write_instruction0(state, RETURN);
	write_instruction1(state, ENDFUNC, decl->func_no);
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

	struct ain_function f = {0};
	f.name = strdup("0");
	int func_no = ain_add_function(state->ain, &f);
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
	toplevel = jaf_parse(out, files, nr_files);
	jaf_resolve_declarations(out, toplevel);

	// Now that type definitions are available, we parse the HLL files.
	assert(nr_hll % 2 == 0);
	for (unsigned i = 0; i < nr_hll; i += 2) {
		struct jaf_block *hll_decl = jaf_parse(out, hll+i, 1);
		jaf_resolve_hll_declarations(out, hll_decl, hll[i+1]);
	}

	// Now that HLL declarations are available, we can do static analysis.
	toplevel = jaf_static_analyze(out, toplevel);
	jaf_compile(out, toplevel);
	jaf_free_block(toplevel);
}
