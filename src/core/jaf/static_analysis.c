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
		jaf_name_append(&ctor->items[0]->fun.name, string_dup(name));
		jaf_process_declarations(env->ain, ctor);
		item->struc.methods = jaf_merge_blocks(item->struc.methods, ctor);
	}
}

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
		jaf_analyze_struct(visitor->env, stmt);
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
