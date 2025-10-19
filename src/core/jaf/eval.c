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
#include <string.h>
#include "system4.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "alice.h"
#include "alice/jaf.h"

static struct jaf_expression *jaf_simplify_negation(struct jaf_expression *in)
{
	struct jaf_expression *expr = in->expr;
	if (expr->type == JAF_EXP_INT) {
		free(in);
		expr->i = -expr->i;
		return expr;
	}
	if (expr->type == JAF_EXP_FLOAT) {
		free(in);
		expr->f = -expr->f;
		return expr;
	}

	return in;
}

static struct jaf_expression *jaf_simplify_bitnot(struct jaf_expression *in)
{
	struct jaf_expression *expr = in->expr;
	if (expr->type == JAF_EXP_INT) {
		free(in);
		expr->i = ~expr->i;
		return expr;
	}

	return in;
}

static struct jaf_expression *jaf_simplify_lognot(struct jaf_expression *in)
{
	struct jaf_expression *expr = in->expr;
	if (expr->type == JAF_EXP_INT) {
		free(in);
		expr->i = !expr->i;
		return expr;
	}

	return in;
}

static struct jaf_expression *jaf_simplify_unary(struct jaf_expression *in)
{
	struct jaf_expression *r;
	switch (in->op) {
	case JAF_UNARY_PLUS:
		r = in->expr;
		free(in);
		return r;
	case JAF_UNARY_MINUS:
		return jaf_simplify_negation(in);
	case JAF_BIT_NOT:
		return jaf_simplify_bitnot(in);
	case JAF_LOG_NOT:
		return jaf_simplify_lognot(in);
	case JAF_AMPERSAND:
	case JAF_PRE_INC:
	case JAF_PRE_DEC:
	case JAF_POST_INC:
	case JAF_POST_DEC:
		return in;
	default:
		break;
	}
	COMPILER_ERROR(in, "Invalid unary operator");
}

static struct jaf_expression *simplify_cast_to_float(struct jaf_expression *e)
{
	if (e->valuetype.data == AIN_FLOAT)
		return e;
	if (e->type == JAF_EXP_INT) {
		float f = e->i;
		e->type = JAF_EXP_FLOAT;
		e->f = f;
		return e;
	}
	return jaf_cast_expression(JAF_FLOAT, e);
}

static struct jaf_expression *jaf_simplify_string_concat(struct jaf_expression *e)
{
	if (e->lhs->type != JAF_EXP_STRING || e->rhs->type != JAF_EXP_STRING)
		return e;
	struct jaf_expression *r = e->lhs;
	string_append(&r->s, e->rhs->s);
	free_string(e->rhs->s);
	free(e->rhs);
	free(e);
	return r;
}

static void jaf_normalize_for_arithmetic(struct jaf_expression *e)
{
	if (e->lhs->valuetype.data == AIN_FLOAT || e->rhs->valuetype.data == AIN_FLOAT) {
		e->lhs = simplify_cast_to_float(e->lhs);
		e->rhs = simplify_cast_to_float(e->rhs);
	}
}

#define SIMPLIFY_ARITHMETIC_FUN(name, op_name, op)			\
	static struct jaf_expression *name(struct jaf_expression *e)	\
	{								\
		jaf_normalize_for_arithmetic(e);			\
		if (e->lhs->type == JAF_EXP_INT && e->rhs->type == JAF_EXP_INT) { \
			struct jaf_expression *r = e->lhs;		\
			r->i = r->i op e->rhs->i;			\
			free(e->rhs);					\
			free(e);					\
			return r;					\
		}							\
		if (e->lhs->type == JAF_EXP_FLOAT && e->rhs->type == JAF_EXP_FLOAT) { \
			struct jaf_expression *r = e->lhs;		\
			r->f = r->f op e->rhs->f;			\
			free(e->rhs);					\
			free(e);					\
			return r;					\
		}							\
		return e;						\
	}

#define SIMPLIFY_INTEGER_FUN(name, op_name, op)				\
	static struct jaf_expression *name(struct jaf_expression *e)	\
	{								\
		if (e->lhs->type == JAF_EXP_INT && e->rhs->type == JAF_EXP_INT) { \
			struct jaf_expression *r = e->lhs;		\
			r->i = r->i op e->rhs->i;			\
			free(e->rhs);					\
			free(e);					\
			return r;					\
		}							\
		return e;						\
	}

SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_multiply,  JAF_MULTIPLY,  *)
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_divide,    JAF_DIVIDE,    /)
SIMPLIFY_INTEGER_FUN   (jaf_simplify_remainder, JAF_REMAINDER, %)
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_plus,      JAF_PLUS,      +) // TODO: string concatentation
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_minus,     JAF_MINUS,     -)
SIMPLIFY_INTEGER_FUN   (jaf_simplify_lshift,    JAF_LSHIFT,    <<)
SIMPLIFY_INTEGER_FUN   (jaf_simplify_rshift,    JAF_RSHIFT,    >>)
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_lt,        JAF_LT,        <)
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_gt,        JAF_GT,        >)
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_lte,       JAF_LTE,       <=)
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_gte,       JAF_GTE,       >=)
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_eq,        JAF_EQ,        ==)
SIMPLIFY_ARITHMETIC_FUN(jaf_simplify_neq,       JAF_NEQ,       !=)
SIMPLIFY_INTEGER_FUN   (jaf_simplify_bitand,    JAF_BIT_AND,   &)
SIMPLIFY_INTEGER_FUN   (jaf_simplify_bitxor,    JAF_BIT_XOR,   ^)
SIMPLIFY_INTEGER_FUN   (jaf_simplify_bitior,    JAF_BIT_IOR,   |)
SIMPLIFY_INTEGER_FUN   (jaf_simplify_logand,    JAF_LOG_AND,   &&)
SIMPLIFY_INTEGER_FUN   (jaf_simplify_logor,     JAF_LOG_OR,    ||)

static struct jaf_expression *jaf_simplify_binary(struct jaf_expression *e)
{
	switch (e->op) {
	case JAF_MULTIPLY:
		return jaf_simplify_multiply(e);
	case JAF_DIVIDE:
		return jaf_simplify_divide(e);
	case JAF_REMAINDER:
		return jaf_simplify_remainder(e);
	case JAF_PLUS:
		if (e->lhs->type == JAF_EXP_STRING)
			return jaf_simplify_string_concat(e);
		else
			return jaf_simplify_plus(e);
	case JAF_MINUS:
		return jaf_simplify_minus(e);
	case JAF_LSHIFT:
		return jaf_simplify_lshift(e);
	case JAF_RSHIFT:
		return jaf_simplify_rshift(e);
	case JAF_LT:
		return jaf_simplify_lt(e);
	case JAF_GT:
		return jaf_simplify_gt(e);
	case JAF_LTE:
		return jaf_simplify_lte(e);
	case JAF_GTE:
		return jaf_simplify_gte(e);
	case JAF_EQ:
		return jaf_simplify_eq(e);
	case JAF_NEQ:
		return jaf_simplify_neq(e);
	case JAF_BIT_AND:
		return jaf_simplify_bitand(e);
	case JAF_BIT_XOR:
		return jaf_simplify_bitxor(e);
	case JAF_BIT_IOR:
		return jaf_simplify_bitior(e);
	case JAF_LOG_AND:
		return jaf_simplify_logand(e);
	case JAF_LOG_OR:
		return jaf_simplify_logor(e);
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
	case JAF_CHAR_ASSIGN:
	case JAF_REQ:
	case JAF_RNE:
		return e;
	default:
		COMPILER_ERROR(e, "Invalid binary operator");
	}
}

static struct jaf_expression *jaf_simplify_ternary(struct jaf_expression *in)
{
	if (in->condition->type == JAF_EXP_INT) {
		if (in->condition->i) {
			jaf_free_expr(in->condition);
			jaf_free_expr(in->alternative);
			struct jaf_expression *r = in->consequent;
			free(in);
			return r;
		} else {
			jaf_free_expr(in->condition);
			jaf_free_expr(in->consequent);
			struct jaf_expression *r = in->alternative;
			free(in);
			return r;
		}
	}

	return in;
}

static struct jaf_expression *jaf_simplify_cast(struct jaf_expression *in)
{
	if (in->cast.type == JAF_INT) {
		if (in->cast.expr->type == JAF_EXP_INT) {
			struct jaf_expression *r = in->cast.expr;
			free(in);
			return r;
		}
		if (in->cast.expr->type == JAF_EXP_FLOAT) {
			struct jaf_expression *r = jaf_integer((int)in->cast.expr->f);
			jaf_free_expr(in);
			return r;
		}
		if (in->cast.expr->type == JAF_EXP_STRING) {
			struct jaf_expression *r = jaf_parse_integer(string_dup(in->cast.expr->s));
			jaf_free_expr(in);
			return r;
		}
	}

	if (in->cast.type == JAF_FLOAT) {
		if (in->cast.expr->type == JAF_EXP_FLOAT) {
			struct jaf_expression *r = in->cast.expr;
			free(in);
			return r;
		}
		if (in->cast.expr->type == JAF_EXP_INT) {
			struct jaf_expression *r = jaf_float((float)in->cast.expr->i);
			jaf_free_expr(in);
			return r;
		}
		if (in->cast.expr->type == JAF_EXP_STRING) {
			struct jaf_expression *r = jaf_parse_float(string_dup(in->cast.expr->s));
			jaf_free_expr(in);
			return r;
		}
	}

	if (in->cast.type == JAF_STRING) {
		if (in->cast.expr->type == JAF_EXP_STRING) {
			struct jaf_expression *r = in->cast.expr;
			free(in);
			return r;
		}
		if (in->cast.expr->type == JAF_EXP_INT) {
			char buf[512];
			snprintf(buf, 512, "%i", in->cast.expr->i);
			jaf_free_expr(in);
			return jaf_string(make_string(buf, strlen(buf)));
		}
		if (in->cast.expr->type == JAF_EXP_FLOAT) {
			char buf[512];
			snprintf(buf, 512, "%f", in->cast.expr->f);
			jaf_free_expr(in);
			return jaf_string(make_string(buf, strlen(buf)));
		}
	}

	return in;
}

static struct jaf_expression *jaf_simplify_char(struct jaf_expression *in)
{
	int c = 0;
	char *s = conv_output(in->s->text);
	int size = strlen(s);
	if (size <= 0)
		goto invalid;
	if (s[0] == '\\') {
		if (size != 2)
			goto invalid;
		switch (s[1]) {
		case '\\': c = '\\'; goto valid;
		case '\'': c = '\''; goto valid;
		default: goto invalid;
		}
	}
	// XXX: assuming output encoding is SJIS
	if (size == 1)
		c = s[0];
	else if (size == 2)
		c = ((uint8_t)s[1] << 8) | (uint8_t)s[0];
	else
		goto invalid;
valid:
	free_string(in->s);
	free(s);
	in->type = JAF_EXP_INT;
	in->i = c;
	return in;
invalid:
	JAF_ERROR(in, "Invalid character constant");
}

/*
 * Simplify an expression by evaluating the constant parts.
 */
struct jaf_expression *jaf_simplify(struct jaf_expression *in)
{
	switch (in->type) {
	case JAF_EXP_VOID:
	case JAF_EXP_INT:
	case JAF_EXP_FLOAT:
	case JAF_EXP_STRING:
	case JAF_EXP_IDENTIFIER:
	case JAF_EXP_THIS:
	case JAF_EXP_FUNCALL:
	case JAF_EXP_SYSCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_INTERFACE_CALL:
	case JAF_EXP_BUILTIN_CALL:
	case JAF_EXP_SUPER_CALL:
	case JAF_EXP_NEW:
	case JAF_EXP_MEMBER:
	case JAF_EXP_SEQ:
	case JAF_EXP_SUBSCRIPT:
	case JAF_EXP_NULL:
	case JAF_EXP_DUMMYREF:
		return in;
	case JAF_EXP_UNARY:
		return jaf_simplify_unary(in);
	case JAF_EXP_BINARY:
		return jaf_simplify_binary(in);
	case JAF_EXP_TERNARY:
		return jaf_simplify_ternary(in);
	case JAF_EXP_CAST:
		return jaf_simplify_cast(in);
	case JAF_EXP_CHAR:
		return jaf_simplify_char(in);
	}
	COMPILER_ERROR(in, "Invalid expression type");
}
