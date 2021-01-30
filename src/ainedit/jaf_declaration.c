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
#include "system4/string.h"
#include "alice.h"
#include "jaf.h"

// jaf_static_analysis.c
void jaf_to_ain_type(struct ain *ain, struct ain_type *out, struct jaf_type_specifier *in);

static void init_variable(struct ain *ain, struct ain_variable *vars, int *var_no, struct jaf_vardecl *decl)
{
	vars[*var_no].name = conv_output(decl->name->text);
	if (ain->version >= 12)
		vars[*var_no].name2 = strdup("");
	jaf_to_ain_type(ain, &vars[*var_no].type, decl->type);
	decl->var_no = *var_no;

	// immediate reference types need extra slot (page+index)
	switch (vars[*var_no].type.data) {
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
		(*var_no)++;
		vars[*var_no].name = strdup("<void>");
		if (ain->version >= 12)
			vars[*var_no].name2 = strdup("");
		vars[*var_no].type.data = AIN_VOID;
		break;
	default:
		break;
	}

	(*var_no)++;
}

struct var_list {
	struct ain *ain;
	struct ain_variable *vars;
	int nr_vars;
};

static void stmt_get_vars(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	struct var_list *vars = visitor->data;

	switch (stmt->kind) {
	case JAF_DECL_VAR:
		if (!(stmt->var.type->qualifiers & JAF_QUAL_CONST)) {
			vars->vars = xrealloc_array(vars->vars, vars->nr_vars, vars->nr_vars+2, sizeof(struct ain_variable));
			init_variable(vars->ain, vars->vars, &vars->nr_vars, &stmt->var);
		}
		break;
	case JAF_DECL_FUN:
		JAF_ERROR(stmt, "Nested Functions not supported");
	default:
		break;
	}
}

static struct ain_variable *block_get_vars(struct ain *ain, struct jaf_block *block, struct ain_variable *vars, int *nr_vars)
{
	struct var_list var_list = {
		.ain = ain,
		.vars = vars,
		.nr_vars = *nr_vars,
	};
	struct jaf_visitor visitor = {
		.visit_stmt_post = stmt_get_vars,
		.data = &var_list,
	};

	jaf_accept_block(block, &visitor);

	*nr_vars = var_list.nr_vars;
	return var_list.vars;
}

static void function_init_vars(struct ain *ain, struct jaf_fundecl *decl, int32_t *nr_args, int32_t *nr_vars, struct ain_variable **vars)
{
	int nr_params = decl->params ? decl->params->nr_items : 0;
	*nr_args = 0;
	*vars = xcalloc(nr_params * 2, sizeof(struct ain_variable));
	for (int i = 0; i < nr_params; i++) {
		assert(decl->params->items[i]->kind == JAF_DECL_VAR);
		assert(decl->params->items[i]->var.name);
		struct jaf_vardecl *param = &decl->params->items[i]->var;
		init_variable(ain, *vars, nr_args, param);
	}
	*nr_vars = *nr_args;
	if (decl->body)
		*vars = block_get_vars(ain, decl->body, *vars, nr_vars);
}

static int _add_function(struct ain *ain, struct jaf_fundecl *decl)
{
	struct ain_function f = {0};
	f.name = conv_output(decl->name->text);
	jaf_to_ain_type(ain, &f.return_type, decl->type);
	function_init_vars(ain, decl, &f.nr_args, &f.nr_vars, &f.vars);
	return ain_add_function(ain, &f);
}

static void copy_type(struct ain_type *dst, struct ain_type *src)
{
	*dst = *src;
	if (ain_is_array_data_type(src->data)) {
		dst->array_type = xcalloc(src->rank, sizeof(struct ain_type));
		for (int i = 0; i < src->rank; i++) {
			copy_type(dst->array_type + i, src->array_type + i);
		}
	}
}

static int copy_function(struct ain *ain, int no)
{
	struct ain_function *src = &ain->functions[no];
	struct ain_function dst = *src;
	dst.name = strdup(src->name);
	copy_type(&dst.return_type, &src->return_type);
	dst.vars = xcalloc(src->nr_vars, sizeof(struct ain_variable));
	for (int i = 0; i < src->nr_vars; i++) {
		dst.vars[i] = src->vars[i];
		dst.vars[i].name = strdup(src->vars[i].name);
		if (src->vars[i].name2) {
			dst.vars[i].name2 = strdup(src->vars[i].name2);
		}
		copy_type(&dst.vars[i].type, &src->vars[i].type);
	}
	return ain_add_function(ain, &dst);
}

static bool types_equal(struct ain_type *a, struct ain_type *b)
{
	if (a->data != b->data)
		return false;
	if (a->data == AIN_STRUCT)
		return a->struc == b->struc;
	return true;
}

static bool function_signatures_equal(struct ain *ain, int _a, int _b)
{
	struct ain_function *a = &ain->functions[_a];
	struct ain_function *b = &ain->functions[_b];
	if (!types_equal(&a->return_type, &b->return_type))
		return false;
	if (a->nr_args != b->nr_args)
		return false;
	for (int i = 0; i < a->nr_args; i++) {
		if (!types_equal(&a->vars[i].type, &b->vars[i].type))
			return false;
	}
	return true;
}

static void override_function(struct ain *ain, struct jaf_block_item *item, int no)
{
	struct jaf_fundecl *decl = &item->fun;
	decl->func_no = no;
	decl->super_no = copy_function(ain, no);
	if (!function_signatures_equal(ain, decl->func_no, decl->super_no))
		JAF_ERROR(item, "Invalid function signature in override of function '%s'", decl->name->text);
	ain->functions[decl->super_no].address = ain->functions[no].address;

	// reinitialize variables of overriden function
	struct ain_function *fun = &ain->functions[no];
	ain_free_variables(fun->vars, fun->nr_vars);
	fun->nr_args = 0;
	fun->nr_vars = 0;
	function_init_vars(ain, decl, &fun->nr_args, &fun->nr_vars, &fun->vars);
}

static void jaf_process_function(struct ain *ain, struct jaf_block_item *item)
{
	struct jaf_fundecl *decl = &item->fun;
	char *name = conv_output(decl->name->text);
	int no = ain_get_function(ain, name);
	free(name);

	if (decl->type->qualifiers & JAF_QUAL_OVERRIDE) {
		if (no <= 0) {
			JAF_ERROR(item, "Function '%s' can't be overridden because it doesn't exist", decl->name->text);
		}
		override_function(ain, item, no);
	} else if (no > 0) {
		JAF_ERROR(item, "Function '%s' already exists", decl->name->text);
	} else {
		decl->func_no = _add_function(ain, decl);
		decl->super_no = 0;
	}

	if (!strcmp(decl->name->text, "main")) {
		if (decl->params || decl->type->type != JAF_INT)
			JAF_ERROR(item, "Invalid signature for main function");
		if (ain->main > 0)
			WARNING("Overriding main function");
		ain->main = decl->func_no;
	} else if (!strcmp(decl->name->text, "message")) {
		if (!decl->params ||
		    decl->params->nr_items != 3 ||
		    decl->params->items[0]->fun.type->type != JAF_INT ||
		    decl->params->items[0]->fun.type->qualifiers ||
		    decl->params->items[1]->fun.type->type != JAF_INT ||
		    decl->params->items[1]->fun.type->qualifiers ||
		    decl->params->items[2]->fun.type->type != JAF_STRING ||
		    decl->params->items[2]->fun.type->qualifiers ||
		    decl->type->type != JAF_VOID) {
			JAF_ERROR(item, "Invalid signature for message function");
		}
		if (ain->msgf > 0)
			WARNING("Overriding message function");
		ain->msgf = decl->func_no;
	}
}

static void jaf_process_functype(struct ain *ain, struct jaf_fundecl *decl)
{
	assert(decl->func_no >= 0 && decl->func_no <= ain->nr_function_types);
	struct ain_function_type *f = &ain->function_types[decl->func_no];
	jaf_to_ain_type(ain, &f->return_type, decl->type);
	function_init_vars(ain, decl, &f->nr_arguments, &f->nr_variables, &f->variables);
}

static void jaf_process_global(struct ain *ain, struct jaf_vardecl *decl)
{
	struct ain_variable v = {0};
	v.name = conv_output(decl->name->text);
	jaf_to_ain_type(ain, &v.type, decl->type);
	ain_add_global(ain, &v);
}

static void jaf_process_structdef(struct ain *ain, struct jaf_block_item *item)
{
	assert(item->struc.struct_no >= 0);
	assert(item->struc.struct_no < ain->nr_structures);

	struct jaf_block *jaf_members = item->struc.members;
	struct ain_variable *members = xcalloc(jaf_members->nr_items, sizeof(struct ain_variable));

	for (size_t i = 0; i < jaf_members->nr_items; i++) {
		if (jaf_members->items[i]->kind != JAF_DECL_VAR)
			continue;

		members[i].name = conv_output(jaf_members->items[i]->var.name->text);
		if (ain->version >= 12)
			members[i].name2 = strdup("");

		jaf_to_ain_type(ain, &members[i].type, jaf_members->items[i]->var.type);
	}
	struct ain_struct *s = &ain->structures[item->struc.struct_no];
	s->nr_members = jaf_members->nr_items;
	s->members = members;
}

void jaf_process_declarations(struct ain *ain, struct jaf_block *block)
{
	for (size_t i = 0; i < block->nr_items; i++) {
		switch (block->items[i]->kind) {
		case JAF_DECL_VAR:
			jaf_process_global(ain, &block->items[i]->var);
			break;
		case JAF_DECL_FUN:
			jaf_process_function(ain, block->items[i]);
			break;
		case JAF_DECL_FUNCTYPE:
			jaf_process_functype(ain, &block->items[i]->fun);
			break;
		case JAF_DECL_STRUCT:
			jaf_process_structdef(ain, block->items[i]);
			break;
		case JAF_EOF:
			break;
		default:
			JAF_ERROR(block->items[i], "Unhandled declaration at top-level: %d",
				  block->items[i]->kind);
		}
	}
}

static void _jaf_process_hll_declaration(struct ain *ain, struct jaf_fundecl *decl, struct ain_hll_function *f)
{
	f->name = xstrdup(decl->name->text);
	jaf_to_ain_type(ain, &f->return_type, decl->type);

	f->nr_arguments = decl->params ? decl->params->nr_items : 0;
	f->arguments = xcalloc(f->nr_arguments, sizeof(struct ain_hll_argument));
	for (int i = 0; i < f->nr_arguments; i++) {
		assert(decl->params->items[i]->kind == JAF_DECL_VAR);
		assert(decl->params->items[i]->var.name);
		f->arguments[i].name = xstrdup(decl->params->items[i]->var.name->text);
		jaf_to_ain_type(ain, &f->arguments[i].type, decl->params->items[i]->var.type);
	}
}

void jaf_process_hll_declarations(struct ain *ain, struct jaf_block *block, const char *hll_name)
{
	struct ain_library lib = {0};
	lib.name = xstrdup(hll_name);
	lib.nr_functions = block->nr_items - 1; // -1 for EOF
	lib.functions = xcalloc(lib.nr_functions, sizeof(struct ain_hll_function));
	for (int i = 0; i < lib.nr_functions; i++) {
		if (block->items[i]->kind != JAF_DECL_FUN)
			JAF_ERROR(block->items[i],
				  "Only function declarations are allowed in HLL files: %d",
				  block->items[i]->kind);
		if (block->items[i]->fun.body)
			JAF_ERROR(block->items[i], "Function definitions not allowed in HLL files");
		_jaf_process_hll_declaration(ain, &block->items[i]->fun, &lib.functions[i]);
	}
	ain_add_library(ain, &lib);
}
