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

#include <assert.h>
#include "jaf.h"

static void jaf_accept_arglist(struct jaf_argument_list *args, struct jaf_visitor *visitor)
{
	for (size_t i = 0; i < args->nr_items; i++) {
		args->items[i] = jaf_accept_expr(args->items[i], visitor);
	}
}

/*
 * NOTE: Supports rewriting expressions via modified return value.
 */
struct jaf_expression *jaf_accept_expr(struct jaf_expression *expr, struct jaf_visitor *visitor)
{
	if (!expr || (!visitor->visit_expr_pre && !visitor->visit_expr_post))
		return expr;

	if (visitor->visit_expr_pre) {
		visitor->expr = expr;
		expr = visitor->visit_expr_pre(expr, visitor);
	}

	switch (expr->type) {
	case JAF_EXP_UNARY:
		expr->expr = jaf_accept_expr(expr->expr, visitor);
		break;
	case JAF_EXP_BINARY:
		expr->lhs = jaf_accept_expr(expr->lhs, visitor);
		expr->rhs = jaf_accept_expr(expr->rhs, visitor);
		break;
	case JAF_EXP_TERNARY:
		expr->condition = jaf_accept_expr(expr->condition, visitor);
		expr->consequent = jaf_accept_expr(expr->consequent, visitor);
		expr->alternative = jaf_accept_expr(expr->alternative, visitor);
		break;
	case JAF_EXP_FUNCALL:
	case JAF_EXP_SYSCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_BUILTIN_CALL:
		expr->call.fun = jaf_accept_expr(expr->call.fun, visitor);
		jaf_accept_arglist(expr->call.args, visitor);
		break;
	case JAF_EXP_NEW:
		jaf_accept_arglist(expr->new.args, visitor);
		break;
	case JAF_EXP_CAST:
		expr->cast.expr = jaf_accept_expr(expr->cast.expr, visitor);
		break;
	case JAF_EXP_MEMBER:
		expr->member.struc = jaf_accept_expr(expr->member.struc, visitor);
		break;
	case JAF_EXP_SEQ:
		expr->seq.head = jaf_accept_expr(expr->seq.head, visitor);
		expr->seq.tail = jaf_accept_expr(expr->seq.tail, visitor);
		break;
	case JAF_EXP_SUBSCRIPT:
		expr->subscript.expr = jaf_accept_expr(expr->subscript.expr, visitor);
		expr->subscript.index = jaf_accept_expr(expr->subscript.index, visitor);
		break;
	case JAF_EXP_CHAR:
	case JAF_EXP_VOID:
	case JAF_EXP_INT:
	case JAF_EXP_FLOAT:
	case JAF_EXP_STRING:
	case JAF_EXP_IDENTIFIER:
	case JAF_EXP_THIS:
		break;
	}

	if (visitor->visit_expr_post) {
		visitor->expr = expr;
		expr = visitor->visit_expr_post(expr, visitor);
	}

	return expr;
}

void jaf_accept_stmt(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	if (!stmt)
		return;

	if (visitor->visit_stmt_pre) {
		visitor->stmt = stmt;
		visitor->visit_stmt_pre(stmt, visitor);
	}

	switch (stmt->kind) {
	case JAF_DECL_VAR:
		if (stmt->var.array_dims) {
			assert(stmt->var.type->type == JAF_ARRAY);
			for (size_t i = 0; i < stmt->var.type->rank; i++) {
				stmt->var.array_dims[i] = jaf_accept_expr(stmt->var.array_dims[i], visitor);
			}
		}
		stmt->var.init = jaf_accept_expr(stmt->var.init, visitor);
		break;
	case JAF_DECL_FUN:
	case JAF_DECL_FUNCTYPE:
		jaf_accept_block(stmt->fun.params, visitor);
		jaf_accept_block(stmt->fun.body, visitor);
		break;
	case JAF_DECL_STRUCT:
		jaf_accept_block(stmt->struc.members, visitor);
		jaf_accept_block(stmt->struc.methods, visitor);
		break;
	case JAF_STMT_LABELED:
		jaf_accept_stmt(stmt->label.stmt, visitor);
		break;
	case JAF_STMT_COMPOUND:
		jaf_accept_block(stmt->block, visitor);
		break;
	case JAF_STMT_EXPRESSION:
		stmt->expr = jaf_accept_expr(stmt->expr, visitor);
		break;
	case JAF_STMT_IF:
		stmt->cond.test = jaf_accept_expr(stmt->cond.test, visitor);
		jaf_accept_stmt(stmt->cond.consequent, visitor);
		jaf_accept_stmt(stmt->cond.alternative, visitor);
		break;
	case JAF_STMT_SWITCH:
		stmt->swi.expr = jaf_accept_expr(stmt->swi.expr, visitor);
		jaf_accept_block(stmt->swi.body, visitor);
		break;
	case JAF_STMT_WHILE:
	case JAF_STMT_DO_WHILE:
		stmt->while_loop.test = jaf_accept_expr(stmt->while_loop.test, visitor);
		jaf_accept_stmt(stmt->while_loop.body, visitor);
		break;
	case JAF_STMT_FOR:
		jaf_accept_block(stmt->for_loop.init, visitor);
		stmt->for_loop.test = jaf_accept_expr(stmt->for_loop.test, visitor);
		stmt->for_loop.after = jaf_accept_expr(stmt->for_loop.after, visitor);
		jaf_accept_stmt(stmt->for_loop.body, visitor);
		break;
	case JAF_STMT_RETURN:
		stmt->expr = jaf_accept_expr(stmt->expr, visitor);
		break;
	case JAF_STMT_CASE:
		stmt->swi_case.expr = jaf_accept_expr(stmt->swi_case.expr, visitor);
		jaf_accept_stmt(stmt->swi_case.stmt, visitor);
		break;
	case JAF_STMT_DEFAULT:
		jaf_accept_stmt(stmt->swi_case.stmt, visitor);
		break;
	case JAF_STMT_RASSIGN:
		stmt->rassign.lhs = jaf_accept_expr(stmt->rassign.lhs, visitor);
		stmt->rassign.rhs = jaf_accept_expr(stmt->rassign.rhs, visitor);
		break;
	case JAF_STMT_GOTO:
	case JAF_STMT_CONTINUE:
	case JAF_STMT_BREAK:
	case JAF_STMT_MESSAGE:
	case JAF_EOF:
		break;
	}

	if (visitor->visit_stmt_post) {
		visitor->stmt = stmt;
		visitor->visit_stmt_post(stmt, visitor);
	}
}

void jaf_accept_block(struct jaf_block *block, struct jaf_visitor *visitor)
{
	if (!block)
		return;
	for (size_t i = 0; i < block->nr_items; i++) {
		jaf_accept_stmt(block->items[i], visitor);
	}
}
