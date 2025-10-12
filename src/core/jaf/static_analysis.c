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

static void analyze_array_allocation(possibly_unused struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	if (!decl->array_dims)
		return;
	for (size_t i = 0; i < decl->type->rank; i++) {
		if (decl->array_dims[i]->type != JAF_EXP_INT)
			JAF_ERROR(item, "Invalid expression as array size");
	}
}

static void analyze_const_declaration(struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	if (!decl->init) {
		JAF_ERROR(item, "const declaration without an initializer");
	}
	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);

	env->locals = xrealloc_array(env->locals, env->nr_locals, env->nr_locals+2,
				     sizeof(struct jaf_env_local));
	env->locals[env->nr_locals].name = decl->name->text;
	env->locals[env->nr_locals].is_const = true;
	jaf_to_initval(&env->locals[env->nr_locals].val, decl->init);
	env->nr_locals++;
}

static void analyze_global_declaration(struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	if (!decl->init)
		return;

	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);
	jaf_check_type(decl->init, &decl->valuetype);

	// add initval to ain object
	int no = ain_add_initval(env->ain, decl->var_no);
	jaf_to_initval(&env->ain->global_initvals[no], decl->init);
	analyze_array_allocation(env, item);
}

void jaf_env_add_local(struct jaf_env *env, char *name, int var_no)
{
	assert(env->func_no >= 0 && env->func_no < env->ain->nr_functions);
	struct ain_function *f = &env->ain->functions[env->func_no];
	assert(var_no >= 0 && var_no < f->nr_vars);
	struct ain_variable *v = &f->vars[var_no];

	env->locals = xrealloc_array(env->locals, env->nr_locals, env->nr_locals+2,
				     sizeof(struct jaf_env_local));
	env->locals[env->nr_locals].name = name;

	switch (v->type.data) {
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
		assert(var_no+1 < f->nr_vars);
		env->locals[env->nr_locals].no = var_no;
		env->locals[env->nr_locals++].var = v;
		env->locals[env->nr_locals].name = "";
		env->locals[env->nr_locals].no = var_no + 1;
		env->locals[env->nr_locals].var = v + 1;
		break;
	default:
		env->locals[env->nr_locals].no = var_no;
		env->locals[env->nr_locals++].var = v;
		break;
	}
}

static void analyze_local_declaration(struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);
	if (decl->init) {
		jaf_check_type(decl->init, &decl->valuetype);
	}
	analyze_array_allocation(env, item);
	jaf_env_add_local(env, decl->name->text, decl->var_no);
}

static void analyze_message(struct jaf_env *env, struct jaf_block_item *item)
{
	if (!item->msg.func) {
		item->msg.func_no = -1;
		return;
	}

	char *u = conv_output(item->msg.func->text);
	if ((item->msg.func_no = ain_get_function(env->ain, u)) < 0)
		JAF_ERROR(item, "Undefined function: %s", item->msg.func->text);
	free(u);
}

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
		visitor->data = env = push_function_env(visitor->data, &stmt->fun);
		jaf_to_ain_type(env->ain, &stmt->fun.valuetype, stmt->fun.type);
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

	switch (stmt->kind) {
	case JAF_DECL_VAR:
		assert(stmt->var.type);
		if (stmt->var.type->qualifiers & JAF_QUAL_CONST) {
			analyze_const_declaration(env, stmt);
		} else if (env->parent) {
			analyze_local_declaration(env, stmt);
		} else {
			analyze_global_declaration(env, stmt);
		}
		break;
	case JAF_STMT_MESSAGE:
		analyze_message(env, stmt);
		break;
	case JAF_STMT_RETURN: {
		assert(env->func_no >= 0 && env->func_no < env->ain->nr_functions);
		struct ain_type *rtype = &env->ain->functions[env->func_no].return_type;
		if (rtype->data == AIN_VOID && stmt->expr)
			JAF_ERROR(stmt, "Return with value in void function");
		if (rtype->data != AIN_VOID && !stmt->expr)
			JAF_ERROR(stmt, "Return without a value in non-void function");
		if (!stmt->expr)
			break;
		if (stmt->expr->type == JAF_EXP_NULL) {
			ain_copy_type(&stmt->expr->valuetype, rtype);
		} else {
			jaf_check_type(stmt->expr, rtype);
		}
		break;
	}
	case JAF_STMT_RASSIGN:
		jaf_check_type_lvalue(env, stmt->rassign.lhs);
		if (!ain_is_ref_data_type(stmt->rassign.lhs->valuetype.data))
			JAF_ERROR(stmt, "LHS of reference assignment is not a reference type");
		if (stmt->rassign.rhs->type == JAF_EXP_NULL) {
			ain_copy_type(&stmt->rassign.rhs->valuetype, &stmt->rassign.lhs->valuetype);
		} else {
			jaf_check_type_lvalue(env, stmt->rassign.rhs);
			jaf_check_type(stmt->rassign.rhs, &stmt->rassign.lhs->valuetype);
		}
		break;
	default:
		break;
	}

	// restore previous scope
	switch (stmt->kind) {
	case JAF_DECL_FUN:
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
	jaf_derive_types(visitor->data, expr);
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
