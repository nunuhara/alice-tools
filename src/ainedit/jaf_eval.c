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
#include "jaf.h"

struct jaf_expression *jaf_simplify(struct jaf_expression *in);

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
	struct jaf_expression *r = in->expr = jaf_simplify(in->expr);
	switch (in->op) {
	case JAF_UNARY_PLUS:
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
	ERROR("Invalid unary operator");
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
	enum jaf_operator op = e->op;
	e->lhs = jaf_simplify(e->lhs);
	e->rhs = jaf_simplify(e->rhs);

	switch (op) {
	case JAF_MULTIPLY:
		return jaf_simplify_multiply(e);
	case JAF_DIVIDE:
		return jaf_simplify_divide(e);
	case JAF_REMAINDER:
		return jaf_simplify_remainder(e);
	case JAF_PLUS:
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
	case JAF_REF_ASSIGN:
		return e;
	default:
		ERROR("Invalid binary operator");
	}
}

static struct jaf_expression *jaf_simplify_ternary(struct jaf_expression *in)
{
	in->condition = jaf_simplify(in->condition);
	in->consequent = jaf_simplify(in->consequent);
	in->alternative = jaf_simplify(in->alternative);

	if (in->condition->type == JAF_EXP_INT) {
		if (in->condition->i) {
			jaf_free_expr(in->condition);
			jaf_free_expr(in->alternative);
			free(in);
			return in->consequent;
		} else {
			jaf_free_expr(in->condition);
			jaf_free_expr(in->consequent);
			free(in);
			return in->alternative;
		}
	}

	return in;
}

static struct jaf_expression *jaf_simplify_funcall(struct jaf_expression *in)
{
	if (in->call.fun)
		in->call.fun = jaf_simplify(in->call.fun);
	if (in->call.args) {
		for (size_t i = 0; i < in->call.args->nr_items; i++) {
			in->call.args->items[i] = jaf_simplify(in->call.args->items[i]);
		}
	}
	return in;
}

static struct jaf_expression *jaf_simplify_cast(struct jaf_expression *in)
{
	in->cast.expr = jaf_simplify(in->cast.expr);

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

static struct jaf_expression *jaf_simplify_member(struct jaf_expression *in)
{
	in->member.struc = jaf_simplify(in->member.struc);
	return in;
}

static struct jaf_expression *jaf_simplify_seq(struct jaf_expression *in)
{
	in->seq.head = jaf_simplify(in->seq.head);
	in->seq.tail = jaf_simplify(in->seq.tail);
	return in;
}

static struct jaf_expression *jaf_simplify_subscript(struct jaf_expression *in)
{
	in->subscript.expr = jaf_simplify(in->subscript.expr);
	in->subscript.index = jaf_simplify(in->subscript.index);
	return in;
}

static struct jaf_expression *jaf_simplify_char(struct jaf_expression *in)
{
	int c = 0;
	int size = in->s->size;
	char *s = in->s->text;
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
	if (size != 1)
		goto invalid;
	c = s[0];
valid:
	free_string(in->s);
	in->type = JAF_EXP_INT;
	in->i = c;
	return in;
invalid:
	ERROR("Invalid character constant");
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
		return in;
	case JAF_EXP_UNARY:
		return jaf_simplify_unary(in);
	case JAF_EXP_BINARY:
		return jaf_simplify_binary(in);
	case JAF_EXP_TERNARY:
		return jaf_simplify_ternary(in);
	case JAF_EXP_FUNCALL:
	case JAF_EXP_SYSCALL:
		return jaf_simplify_funcall(in);
	case JAF_EXP_CAST:
		return jaf_simplify_cast(in);
	case JAF_EXP_MEMBER:
		return jaf_simplify_member(in);
	case JAF_EXP_SEQ:
		return jaf_simplify_seq(in);
	case JAF_EXP_SUBSCRIPT:
		return jaf_simplify_subscript(in);
	case JAF_EXP_CHAR:
		return jaf_simplify_char(in);
	}
	ERROR("Invalid expression type");
}

struct jaf_expression *jaf_compute_constexpr(struct jaf_expression *in)
{
	struct jaf_expression *out = jaf_simplify(in);
	switch (out->type) {
	case JAF_EXP_INT:
	case JAF_EXP_FLOAT:
	case JAF_EXP_STRING:
		return out;
	default:
		return NULL;
	}
}
