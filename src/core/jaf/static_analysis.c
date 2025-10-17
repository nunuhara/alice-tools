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
#include <assert.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/jaf.h"

/*
 * Create a new empty scope.
 */
static struct jaf_env *push_env(struct jaf_env *parent)
{
	struct jaf_env *newenv = xcalloc(1, sizeof(struct jaf_env));
	newenv->ain = parent->ain;
	newenv->parent = parent;
	newenv->func_no = parent->func_no;
	newenv->fundecl = parent->fundecl;
	return newenv;
}

/*
 * Discard the current scope and return to the parent scope.
 */
static struct jaf_env *pop_env(struct jaf_env *env)
{
	struct jaf_env *parent = env->parent;
	free(env->locals);
	free(env);
	return parent;
}

/*
 * Create a new scope for the body of a function call.
 */
static struct jaf_env *push_function_env(struct jaf_env *parent, struct jaf_fundecl *decl)
{
	// create new scope with function arguments
	assert(decl->func_no >= 0);
	assert(decl->func_no < parent->ain->nr_functions);
	struct ain_function *fun = &parent->ain->functions[decl->func_no];
	struct jaf_env *funenv = push_env(parent);
	funenv->func_no = decl->func_no;
	funenv->fundecl = decl;
	funenv->nr_locals = fun->nr_args;
	funenv->locals = xcalloc(funenv->nr_locals, sizeof(struct jaf_env_local));
	for (size_t i = 0; i < funenv->nr_locals; i++) {
		funenv->locals[i].name = fun->vars[i].name;
		funenv->locals[i].no = i;
		funenv->locals[i].var = &fun->vars[i];
	}

	return funenv;
}

/*
 * This function sets up a new scope before the child nodes are analyzed (as required).
 */
static void jaf_analyze_stmt_pre(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	struct jaf_env *env;
	// create a new scope
	switch (stmt->kind) {
	case JAF_DECL_FUN:
		if (stmt->fun.body) {
			visitor->data = env = push_function_env(visitor->data, &stmt->fun);
			jaf_to_ain_type(env->ain, &stmt->fun.valuetype, stmt->fun.type);
		}
		break;
	case JAF_STMT_COMPOUND:
	case JAF_STMT_SWITCH:
	case JAF_STMT_FOR:
		visitor->data = push_env(visitor->data);
		break;
	default:
		break;
	}
}

static void jaf_analyze_stmt_post(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	struct jaf_env *env = visitor->data;

	jaf_type_check_statement(env, stmt);

	// restore previous scope
	switch (stmt->kind) {
	case JAF_DECL_FUN:
		if (!stmt->fun.body)
			break;
		// fallthrough
	case JAF_STMT_COMPOUND:
	case JAF_STMT_SWITCH:
	case JAF_STMT_FOR:
		stmt->is_scope = true;
		visitor->data = pop_env(env);
		break;
	default:
		break;
	}
}

static struct jaf_expression *jaf_analyze_expr(struct jaf_expression *expr, struct jaf_visitor *visitor)
{
	if (expr->type == JAF_EXP_NEW) {
		visitor->stmt->is_scope = true;
	}
	jaf_type_check_expression(visitor->data, expr);
	return jaf_simplify(expr);
}

struct jaf_block *jaf_static_analyze(struct ain *ain, struct jaf_block *block)
{
	struct jaf_env env = {
		.ain = ain,
		.parent = NULL,
		.locals = NULL
	};
	struct jaf_visitor visitor = {
		.visit_stmt_pre = jaf_analyze_stmt_pre,
		.visit_stmt_post = jaf_analyze_stmt_post,
		.visit_expr_post = jaf_analyze_expr,
		.data = &env,
	};

	jaf_accept_block(block, &visitor);
	free(env.locals);

	return block;
}
