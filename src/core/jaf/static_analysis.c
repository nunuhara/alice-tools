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

static void jaf_analyze_stmt_pre(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	if (stmt->kind == JAF_DECL_FUN && stmt->fun.body) {
		jaf_to_ain_type(visitor->env->ain, &stmt->fun.valuetype, stmt->fun.type);
	}
	if (stmt->kind == JAF_DECL_VAR && stmt->var.type->type == JAF_OPTION
			&& AIN_VERSION_LT(visitor->env->ain, 14, 0)) {
		JAF_ERROR(stmt, "option<> variables not supported in this .ain version");
	}
}

static void jaf_analyze_stmt_post(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	jaf_type_check_statement(visitor->env, stmt);

	switch (stmt->kind) {
	case JAF_DECL_VAR:
		jaf_type_check_vardecl(visitor->env, stmt);
		break;
	case JAF_DECL_STRUCT:
		// TODO: check that methods declared in struct have implementation
		//jaf_analyze_struct(visitor->env, stmt);
		break;
	case JAF_DECL_INTERFACE:
		// TODO: For interfaces that were already defined in input .ain file, check
		//       that iface declaration is compatible with existing implementations.
		//jaf_analyze_interface(visitor->env, stmt);
		break;
	case JAF_DECL_FUN:
		if (!stmt->fun.body)
			break;
		// fallthrough
	case JAF_STMT_COMPOUND:
	case JAF_STMT_SWITCH:
	case JAF_STMT_FOR:
		stmt->is_scope = true;
		break;
	default:
		break;
	}
}

static struct jaf_expression *jaf_analyze_expr(struct jaf_expression *expr, struct jaf_visitor *visitor)
{
	jaf_type_check_expression(visitor->env, expr);
	return jaf_simplify(expr);
}

static struct ain_hll_function *add_library_function(struct ain_library *lib, const char *name,
		int *fun_no_out)
{
	lib->functions = xrealloc_array(lib->functions, lib->nr_functions, lib->nr_functions+1,
			sizeof(struct ain_hll_function));
	struct ain_hll_function *f = &lib->functions[lib->nr_functions];
	f->name = xstrdup(name);
	*fun_no_out = lib->nr_functions++;
	return f;
}

static void library_function_set_nr_arguments(struct ain_hll_function *f, int nr_args)
{
	f->nr_arguments = nr_args;
	f->arguments = xcalloc(nr_args, sizeof(struct ain_hll_argument));
}

#define HLL_ARG(str, t) \
	(struct ain_hll_argument) { \
		.name = xstrdup(str), \
		.type = { .data = t, .struc = -1 } \
	}

static void init_array_self_param(struct ain *ain, struct ain_hll_argument *arg)
{
	*arg = HLL_ARG("self", AIN_REF_ARRAY);
	if (AIN_VERSION_GTE(ain, 14, 0)) {
		arg->type.rank = 1;
		arg->type.array_type = xcalloc(1, sizeof(struct ain_type));
		arg->type.array_type->data = AIN_HLL_PARAM;
		arg->type.array_type->struc = -1;
	}
}

static struct ain_library *get_or_add_library(struct ain *ain, const char *name, int *lib_no_out)
{
	int lib_no = ain_get_library(ain, name);
	if (lib_no < 0)
		lib_no = ain_add_library(ain, name);
	*lib_no_out = lib_no;
	assert(lib_no >= 0 && lib_no < ain->nr_libraries);
	return &ain->libraries[lib_no];
}

static void add_delegate_hll_fun(struct ain *ain, int lib_no, enum ain_data_type rtype,
		const char *name, int nr_args)
{
	int fun_no = ain_get_library_function(ain, lib_no, name);
	if (fun_no >= 0)
		return;

	struct ain_hll_function *f = add_library_function(&ain->libraries[lib_no], name, &fun_no);
	f->return_type = (struct ain_type) { .data = rtype, .struc = -1 };
	library_function_set_nr_arguments(f, nr_args);
	f->arguments[0] = HLL_ARG("Self", AIN_REF_DELEGATE);
	if (nr_args > 1)
		f->arguments[1] = HLL_ARG("Func", AIN_HLL_FUNC);
}

static void jaf_check_builtin_hll(struct ain *ain)
{
	const struct ain_type void_type = { .data = AIN_VOID, .struc = -1 };
	// ensure that Array.Alloc and Array.Free exist, because calls to them are
	// generated implicitly
	if (AIN_VERSION_GTE(ain, 11, 0)) {
		// array builtins
		int lib_no, fun_no;
		struct ain_library *lib = get_or_add_library(ain, "Array", &lib_no);
		fun_no = ain_get_library_function(ain, lib_no, "Alloc");
		if (fun_no < 0) {
			struct ain_hll_function *f = add_library_function(lib, "Alloc", &fun_no);
			f->return_type = void_type;
			if (AIN_VERSION_GTE(ain, 14, 0)) {
				library_function_set_nr_arguments(f, 2);
				init_array_self_param(ain, &f->arguments[0]);
				f->arguments[1] = HLL_ARG("Numof", AIN_INT);
			} else {
				library_function_set_nr_arguments(f, 5);
				init_array_self_param(ain, &f->arguments[0]);
				f->arguments[1] = HLL_ARG("Numof", AIN_INT);
				f->arguments[2] = HLL_ARG("Numof2", AIN_INT);
				f->arguments[3] = HLL_ARG("Numof3", AIN_INT);
				f->arguments[4] = HLL_ARG("Numof4", AIN_INT);
			}
		}
		fun_no = ain_get_library_function(ain, lib_no, "Free");
		if (fun_no < 0) {
			struct ain_hll_function *f = add_library_function(lib, "Free", &fun_no);
			f->return_type = void_type;
			f->nr_arguments = 1;
			f->arguments = xcalloc(1, sizeof(struct ain_hll_argument));
			init_array_self_param(ain, &f->arguments[0]);
		}
	}
	if (AIN_VERSION_GTE(ain, 12, 0)) {
		// delegate builtins
		int lib_no;
		get_or_add_library(ain, "Delegate", &lib_no);
		const char *exist_name = AIN_VERSION_GTE(ain, 14, 0) ? "IsExist" : "Exist";
		add_delegate_hll_fun(ain, lib_no, AIN_VOID, "Set", 2);
		add_delegate_hll_fun(ain, lib_no, AIN_VOID, "Add", 2);
		add_delegate_hll_fun(ain, lib_no, AIN_INT, "Numof", 1);
		add_delegate_hll_fun(ain, lib_no, AIN_BOOL, "Empty", 1);
		add_delegate_hll_fun(ain, lib_no, AIN_BOOL, exist_name, 2);
		add_delegate_hll_fun(ain, lib_no, AIN_VOID, "Erase", 2);
		add_delegate_hll_fun(ain, lib_no, AIN_VOID, "Clear", 1);
	}
}

struct jaf_block *jaf_static_analyze(struct ain *ain, struct jaf_block *block)
{
	jaf_check_builtin_hll(ain);

	struct jaf_visitor visitor = {
		.visit_stmt_pre = jaf_analyze_stmt_pre,
		.visit_stmt_post = jaf_analyze_stmt_post,
		.visit_expr_post = jaf_analyze_expr,
	};

	jaf_accept_block(ain, block, &visitor);
	return block;
}
