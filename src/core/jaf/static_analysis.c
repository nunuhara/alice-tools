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
struct jaf_env *jaf_env_push(struct jaf_env *parent)
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
struct jaf_env *jaf_env_pop(struct jaf_env *env)
{
	struct jaf_env *parent = env->parent;
	free(env->locals);
	free(env);
	return parent;
}

/*
 * Create a new scope for the body of a function call.
 */
struct jaf_env *jaf_env_push_function(struct jaf_env *parent, struct jaf_fundecl *decl)
{
	// create new scope with function arguments
	struct jaf_env *funenv = jaf_env_push(parent);
	funenv->func_no = decl->func_no;
	funenv->fundecl = decl;
	funenv->nr_locals = decl->params ? decl->params->nr_items : 0;
	if (funenv->nr_locals) {
		funenv->locals = xcalloc(funenv->nr_locals, sizeof(struct jaf_env_local));
		for (size_t i = 0; i < funenv->nr_locals; i++) {
			struct jaf_block_item *param = decl->params->items[i];
			assert(param->kind == JAF_DECL_VAR);
			funenv->locals[i].name = param->var.name->text;
			funenv->locals[i].decl = &param->var;
		}
	}

	return funenv;
}

void jaf_env_add_local(struct jaf_env *env, struct jaf_vardecl *decl)
{
	env->locals = xrealloc_array(env->locals, env->nr_locals, env->nr_locals+1,
				     sizeof(struct jaf_env_local));
	env->locals[env->nr_locals++] = (struct jaf_env_local) {
		.name = decl->name->text,
		.decl = decl,
	};
}

static struct jaf_env_local *jaf_scope_lookup(struct jaf_env *env, const char *name)
{
	for (size_t i = 0; i < env->nr_locals; i++) {
		if (!strcmp(env->locals[i].name, name)) {
			return &env->locals[i];
		}
	}
	return NULL;
}

struct jaf_env_local *jaf_env_lookup(struct jaf_env *env, const char *name)
{
	struct jaf_env *scope = env;
        while (scope) {
		struct jaf_env_local *v = jaf_scope_lookup(scope, name);
		if (v) {
			return v;
		}
		scope = scope->parent;
	}
	return NULL;
}

static int check_iface_method(struct ain *ain, struct ain_function_type *m, struct ain_struct *s)
{
	struct string *name = make_string(s->name, strlen(s->name));
	string_push_back(&name, '@');
	string_append_cstr(&name, m->name, strlen(m->name));
	int no = ain_get_function(ain, name->text);
	free_string(name);
	return no;
}

static void jaf_analyze_struct(struct jaf_env *env, struct jaf_block_item *item)
{
	assert(item->struc.struct_no >= 0 && item->struc.struct_no < env->ain->nr_structures);
	struct ain_struct *s = &env->ain->structures[item->struc.struct_no];

	s->nr_interfaces = kv_size(item->struc.interfaces);
	s->interfaces = xcalloc(s->nr_interfaces, sizeof(struct ain_interface));

	int iface_index = 0;
	int vtable_offset = 0;
	struct string *p;
	kv_foreach(p, item->struc.interfaces) {
		char *name = conv_output(p->text);
		int iface_no = ain_get_struct(env->ain, name);
		free(name);
		if (iface_no < 0)
			JAF_ERROR(item, "Undefined interface: %s", p->text);
		struct ain_struct *iface = &env->ain->structures[iface_no];
		if (!iface->is_interface)
			JAF_ERROR(item, "Not an interface: %s", p->text);

		// write interface data to struct
		s->interfaces[iface_index].struct_type = iface_no;
		s->interfaces[iface_index].vtable_offset = vtable_offset;

		for (int i = 0; i < iface->nr_iface_methods; i++) {
			// check that method with correct signature exists for struct
			int mno = check_iface_method(env->ain, &iface->iface_methods[i], s);
			if (mno < 0)
				JAF_ERROR(item, "Interface method not implemented: %s::%s",
						item->struc.name->text, p->text);
		}

		iface_index++;
		vtable_offset += iface->nr_iface_methods;
	}

	// generate constructor if needed and none were defined
	if (s->nr_interfaces > 0 && s->constructor <= 0) {
		struct string *name = string_dup(item->struc.name);
		struct jaf_block *ctor = jaf_constructor(name, jaf_block(NULL));
		jaf_process_declarations(env->ain, ctor);
		item->struc.methods = jaf_merge_blocks(item->struc.methods, ctor);
	}
}

/*
 * This function sets up a new scope before the child nodes are analyzed (as required).
 */
static void jaf_analyze_stmt_pre(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	struct jaf_env *env = visitor->data;
	// create a new scope
	switch (stmt->kind) {
	case JAF_DECL_FUN:
		if (stmt->fun.body) {
			visitor->data = env = jaf_env_push_function(visitor->data, &stmt->fun);
			jaf_to_ain_type(env->ain, &stmt->fun.valuetype, stmt->fun.type);
		}
		break;
	case JAF_STMT_COMPOUND:
	case JAF_STMT_SWITCH:
	case JAF_STMT_FOR:
		visitor->data = jaf_env_push(visitor->data);
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
	case JAF_DECL_VAR:
		jaf_type_check_vardecl(env, stmt);
		break;
	case JAF_DECL_STRUCT:
		jaf_analyze_struct(env, stmt);
		break;
	// TODO: For interfaces that were already defined in input .ain file, check
	//       that iface declaration is compatible with existing implementations.
	case JAF_DECL_FUN:
		if (!stmt->fun.body)
			break;
		// fallthrough
	case JAF_STMT_COMPOUND:
	case JAF_STMT_SWITCH:
	case JAF_STMT_FOR:
		stmt->is_scope = true;
		visitor->data = jaf_env_pop(env);
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
