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

static int add_library_function(struct ain_library *lib)
{
	lib->functions = xrealloc_array(lib->functions, lib->nr_functions, lib->nr_functions+1,
			sizeof(struct ain_hll_function));
	return lib->nr_functions++;
}

#define HLL_ARG(str, t) \
	(struct ain_hll_argument) { \
		.name = xstrdup(str), \
		.type = { .data = t, .struc = -1 } \
	}

static void jaf_check_builtin_hll(struct ain *ain)
{
	// ensure that Array.Alloc and Array.Free exist, because calls to them are
	// generated implicitly
	if (AIN_VERSION_GTE(ain, 11, 0)) {
		int array_hll = ain_get_library(ain, "Array");
		if (array_hll < 0)
			array_hll = ain_add_library(ain, "Array");
		assert(array_hll >= 0 && array_hll < ain->nr_libraries);
		struct ain_library *lib = &ain->libraries[array_hll];
		int alloc_fno = ain_get_library_function(ain, array_hll, "Alloc");
		if (alloc_fno < 0) {
			alloc_fno = add_library_function(lib);
			struct ain_hll_function *f = &lib->functions[alloc_fno];
			f->name = xstrdup("Alloc");
			f->return_type.data = AIN_VOID;
			f->return_type.struc = -1;
			f->nr_arguments = 5;
			f->arguments = xcalloc(5, sizeof(struct ain_hll_argument));
			f->arguments[0] = HLL_ARG("self", AIN_REF_ARRAY);
			f->arguments[1] = HLL_ARG("Numof", AIN_INT);
			f->arguments[2] = HLL_ARG("Numof2", AIN_INT);
			f->arguments[3] = HLL_ARG("Numof3", AIN_INT);
			f->arguments[4] = HLL_ARG("Numof4", AIN_INT);
		}
		int free_fno = ain_get_library_function(ain, array_hll, "Free");
		if (free_fno < 0) {
			free_fno = add_library_function(lib);
			struct ain_hll_function *f = &lib->functions[free_fno];
			f->name = xstrdup("Free");
			f->return_type.data = AIN_VOID;
			f->return_type.struc = -1;
			f->nr_arguments = 1;
			f->arguments = xcalloc(1, sizeof(struct ain_hll_argument));
			f->arguments[0] = HLL_ARG("self", AIN_REF_ARRAY);
		}
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
