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
#include "alice/jaf.h"

// jaf_static_analysis.c
void jaf_to_ain_type(struct ain *ain, struct ain_type *out, struct jaf_type_specifier *in);

struct string *jaf_name_collapse(struct ain *ain, struct jaf_name *name)
{
	if (name->collapsed)
		return name->collapsed;

	assert(name->nr_parts >= 1);
	name->collapsed = string_dup(name->parts[0]);
	if (name->nr_parts == 1)
		return name->collapsed;

	for (size_t i = 1; i < name->nr_parts - 1; i++) {
		string_push_back(&name->collapsed, ':');
		string_push_back(&name->collapsed, ':');
		string_append(&name->collapsed, name->parts[i]);
	}

	// XXX: static methods don't have '@' in them, so we check for this case first
	//      (e.g. 'SkillEffectProcessFactory::CreateOne' in Rance 10).
	struct string *tmp = string_dup(name->collapsed);
	string_push_back(&tmp, ':');
	string_push_back(&tmp, ':');
	string_append(&tmp, name->parts[name->nr_parts - 1]);
	char *tmp_conv = conv_output(tmp->text);
	if (ain_get_function(ain, tmp_conv) > 0) {
		free_string(name->collapsed);
		free(tmp_conv);
		name->collapsed = tmp;
		return name->collapsed;
	}
	free_string(tmp);
	free(tmp_conv);

	// check if name is method
	if (ain) {
		char *str = conv_output(name->collapsed->text);
		name->struct_no = ain_get_struct(ain, str);
		if (name->struct_no >= 0) {
			// method
			string_push_back(&name->collapsed, '@');
		} else {
			string_push_back(&name->collapsed, ':');
			string_push_back(&name->collapsed, ':');
		}
		free(str);
	} else {
		name->struct_no = -1;
		string_push_back(&name->collapsed, ':');
		string_push_back(&name->collapsed, ':');
	}

	if (name->struct_no >= 0) {
		// check if method is constructor/destructor
		struct string *class_name = name->parts[name->nr_parts - 2];
		struct string *method_name = name->parts[name->nr_parts - 1];
		if (method_name->text[0] == '~' && !strcmp(class_name->text, method_name->text + 1)) {
			// destructor
			string_push_back(&name->collapsed, '1');
			name->is_destructor = true;
		} else if (!strcmp(class_name->text, method_name->text)) {
			// constructor
			string_push_back(&name->collapsed, '0');
			name->is_constructor = true;
		} else {
			// regular method
			string_append(&name->collapsed, name->parts[name->nr_parts - 1]);
		}
	} else {
		string_append(&name->collapsed, name->parts[name->nr_parts - 1]);
	}
	//printf("collapsed name = %s\n", name->collapsed->text);
	return name->collapsed;
}

struct var_list {
	struct ain *ain;
	struct ain_variable *vars;
	int nr_vars;
};

static warn_unused int _init_variable(struct var_list *list, const char *name,
		struct ain_type *type)
{
	int var = list->nr_vars;
	list->vars[var].name = conv_output(name);
	if (list->ain->version >= 12)
		list->vars[var].name2 = strdup("");
	list->vars[var].type = *type;

	// immediate reference types need extra slot (page+index)
	switch (list->vars[var].type.data) {
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
	case AIN_IFACE:
		list->nr_vars++;
		list->vars[var+1].name = strdup("<void>");
		if (list->ain->version >= 12)
			list->vars[var+1].name2 = strdup("");
		list->vars[var+1].type.data = AIN_VOID;
		break;
	default:
		break;
	}

	list->nr_vars++;
	return var;
}

static warn_unused int init_variable(struct var_list *list, const char *name, struct jaf_type_specifier *type)
{
	struct ain_type atype;
	jaf_to_ain_type(list->ain, &atype, type);
	return _init_variable(list, name, &atype);
}

static void stmt_get_vars(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	struct var_list *vars = visitor->data;

	switch (stmt->kind) {
	case JAF_DECL_VAR:
		if (!(stmt->var.type->qualifiers & JAF_QUAL_CONST)) {
			vars->vars = xrealloc_array(vars->vars, vars->nr_vars, vars->nr_vars+2, sizeof(struct ain_variable));
			stmt->var.var = init_variable(vars, stmt->var.name->text, stmt->var.type);
		}
		break;
	case JAF_DECL_FUN:
		JAF_ERROR(stmt, "Nested Functions not supported");
	default:
		break;
	}
}

static int _add_dummy_variable(struct var_list *vars, struct ain_type *type, const char *fmt, ...)
{
	char name[1024];
	char dfmt[1024];

	snprintf(dfmt, 1023, "<dummy : %s>", fmt);

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(name, 1023, dfmt, ap);
	va_end(ap);

	vars->vars = xrealloc_array(vars->vars, vars->nr_vars, vars->nr_vars+2, sizeof(struct ain_variable));
	struct ain_type copy;
	ain_copy_type(&copy, type);
	return _init_variable(vars, name, &copy);
}

static int add_dummy_variable(struct var_list *vars, struct jaf_type_specifier *type, const char *fmt, ...)
{
	char name[1024];
	char dfmt[1024];

	snprintf(dfmt, 1023, "<dummy : %s>", fmt);

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(name, 1023, dfmt, ap);
	va_end(ap);

	vars->vars = xrealloc_array(vars->vars, vars->nr_vars, vars->nr_vars+2, sizeof(struct ain_variable));
	return init_variable(vars, name, type);
}

static struct jaf_expression *expr_get_vars(struct jaf_expression *expr, struct jaf_visitor *visitor)
{
	struct var_list *vars = visitor->data;

	struct jaf_expression *tmp;
	switch (expr->type) {
	case JAF_EXP_NEW:
		assert(expr->new.type->name);
		tmp = jaf_expr(JAF_EXP_DUMMYREF, 0);
		tmp->dummy.expr = expr;
		tmp->dummy.var_no = add_dummy_variable(vars, expr->new.type, "new %s",
				expr->new.type->name->text);
		ain_copy_type(&tmp->valuetype, &expr->valuetype);
		expr = tmp;
		break;
	case JAF_EXP_FUNCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_INTERFACE_CALL:
	case JAF_EXP_SUPER_CALL:
	case JAF_EXP_HLLCALL:
		if (!ain_is_ref_data_type(expr->valuetype.data))
			break;
		tmp = jaf_expr(JAF_EXP_DUMMYREF, 0);
		tmp->dummy.expr = expr;
		tmp->dummy.var_no = _add_dummy_variable(vars, &expr->valuetype, "return");
		ain_copy_type(&tmp->valuetype, &expr->valuetype);
		expr = tmp;
		break;
	default:
		break;
	}

	return expr;
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
		.visit_expr_post = expr_get_vars,
		.data = &var_list,
	};

	jaf_accept_block(block, &visitor);

	*nr_vars = var_list.nr_vars;
	return var_list.vars;
}

void jaf_allocate_variables(struct ain *ain, struct jaf_block *block)
{
	for (size_t i = 0; i < block->nr_items; i++) {
		struct jaf_block_item *item = block->items[i];
		if (item->kind == JAF_DECL_FUN && item->fun.body) {
			assert(item->fun.func_no > 0 && item->fun.func_no <= ain->nr_functions);
			struct ain_function *f = &ain->functions[item->fun.func_no];
			f->vars = block_get_vars(ain, item->fun.body, f->vars, &f->nr_vars);
		} else if (item->kind == JAF_DECL_STRUCT) {
			jaf_allocate_variables(ain, item->struc.methods);
		}
	}
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
		struct var_list var_list = {
			.ain = ain,
			.vars = *vars,
			.nr_vars = *nr_args
		};
		param->var = init_variable(&var_list, param->name->text, param->type);
		*nr_args = var_list.nr_vars;
	}
	*nr_vars = *nr_args;
}

static bool types_equal(struct ain_type *a, struct ain_type *b)
{
	if (a->data != b->data)
		return false;
	if (a->data == AIN_STRUCT)
		return a->struc == b->struc;
	return true;
}

static bool function_signatures_equal(struct ain *ain, struct ain_function *a, struct ain_function *b)
{
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
	decl->super_no = ain_dup_function(ain, no);
	if (!function_signatures_equal(ain, &ain->functions[decl->func_no],
				&ain->functions[decl->super_no]))
		JAF_ERROR(item, "Invalid function signature in override of function '%s'",
				jaf_name_collapse(ain, &decl->name)->text);
	if (!decl->body)
		JAF_ERROR(item, "Function override without body");
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

	struct string *name_str = jaf_name_collapse(ain, &decl->name);
	char *name = conv_output(name_str->text);
	int no = ain_get_function(ain, name);
	struct ain_function *existing = no >= 0 ? &ain->functions[no] : NULL;

	if (decl->type->qualifiers & JAF_QUAL_OVERRIDE) {
		if (!existing) {
			JAF_ERROR(item, "Function '%s' can't be overridden because it doesn't exist",
					name_str->text);
		}
		override_function(ain, item, no);
	} else if (existing) {
		if (existing->crc && decl->body)
			JAF_ERROR(item, "Multiple definitions of function '%s'", name_str->text);
		decl->func_no = no;
		struct ain_function f = {0};
		jaf_to_ain_type(ain, &f.return_type, decl->type);
		function_init_vars(ain, decl, &f.nr_args, &f.nr_vars, &f.vars);
		if (!function_signatures_equal(ain, existing, &f))
			JAF_ERROR(item, "Incompatible declarations of function '%s'", name_str->text);
		if (decl->body)
			existing->crc = 1;
		ain_free_variables(f.vars, f.nr_vars);
		ain_free_type(&f.return_type);
	} else {
		decl->func_no = ain_add_function(ain, name);
		struct ain_function *f = &ain->functions[decl->func_no];
		jaf_to_ain_type(ain, &f->return_type, decl->type);
		function_init_vars(ain, decl, &f->nr_args, &f->nr_vars, &f->vars);
		f->struct_type = decl->name.struct_no;
		decl->super_no = 0;
		if (decl->name.is_constructor || decl->name.is_destructor) {
			assert(decl->name.struct_no >= 0 && decl->name.struct_no < ain->nr_structures);
			struct ain_struct *s = &ain->structures[decl->name.struct_no];
			if (decl->name.is_constructor)
				s->constructor = decl->func_no;
			else
				s->destructor = decl->func_no;
		}
		if (decl->body)
			f->crc = 1; // XXX: hack to allow checking for multiple definitions
	}

	if (!strcmp(name_str->text, "main")) {
		if (decl->params || decl->type->type != JAF_INT)
			JAF_ERROR(item, "Invalid signature for main function");
		if (ain->main > 0)
			WARNING("Overriding main function");
		ain->main = decl->func_no;
	} else if (!strcmp(name_str->text, "message")) {
		if (!decl->params ||
		    decl->params->nr_items != 3 ||
		    decl->params->items[0]->var.type->type != JAF_INT ||
		    decl->params->items[0]->var.type->qualifiers ||
		    decl->params->items[1]->var.type->type != JAF_INT ||
		    decl->params->items[1]->var.type->qualifiers ||
		    decl->params->items[2]->var.type->type != JAF_STRING ||
		    decl->params->items[2]->var.type->qualifiers ||
		    decl->type->type != JAF_VOID) {
			JAF_ERROR(item, "Invalid signature for message function");
		}
		if (ain->msgf > 0)
			WARNING("Overriding message function");
		ain->msgf = decl->func_no;
	}
	free(name);
}

static void jaf_process_functype(struct ain *ain, struct jaf_fundecl *decl)
{
	assert(decl->func_no >= 0 && decl->func_no <= ain->nr_function_types);
	struct ain_function_type *f = &ain->function_types[decl->func_no];
	jaf_to_ain_type(ain, &f->return_type, decl->type);
	function_init_vars(ain, decl, &f->nr_arguments, &f->nr_variables, &f->variables);
}

static void jaf_process_delegate(struct ain *ain, struct jaf_fundecl *decl)
{
	assert(decl->func_no >= 0 && decl->func_no <= ain->nr_delegates);
	struct ain_function_type *f = &ain->delegates[decl->func_no];
	jaf_to_ain_type(ain, &f->return_type, decl->type);
	function_init_vars(ain, decl, &f->nr_arguments, &f->nr_variables, &f->variables);
}

static void jaf_process_global(struct ain *ain, struct jaf_vardecl *decl)
{
	char *tmp = conv_output(decl->name->text);
	decl->var = ain_add_global(ain, tmp);
	free(tmp);

	jaf_to_ain_type(ain, &ain->globals[decl->var].type, decl->type);
}

static void jaf_process_structdef(struct ain *ain, struct jaf_block_item *item)
{
	assert(item->struc.struct_no >= 0);
	assert(item->struc.struct_no < ain->nr_structures);
	struct ain_struct *s = &ain->structures[item->struc.struct_no];

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
	for (size_t i = 0; i < item->struc.methods->nr_items; i++) {
		struct jaf_block_item *method = item->struc.methods->items[i];
		assert(method->kind == JAF_DECL_FUN);
		jaf_name_prepend(&method->fun.name, string_dup(item->struc.name));
		jaf_process_function(ain, method);
	}
	s->nr_members = jaf_members->nr_items;
	s->members = members;
}

static void jaf_process_interface_method(struct ain *ain, struct ain_function_type *ft,
		struct jaf_fundecl *decl)
{
	jaf_to_ain_type(ain, &ft->return_type, decl->type);
	function_init_vars(ain, decl, &ft->nr_arguments, &ft->nr_variables, &ft->variables);
}

static void jaf_process_interface(struct ain *ain, struct jaf_block_item *item)
{
	int struct_no = item->struc.struct_no;
	struct jaf_block *methods = item->struc.methods;

	assert(struct_no >= 0 && struct_no < ain->nr_structures);
	struct ain_struct *s = &ain->structures[struct_no];
	s->nr_iface_methods = methods->nr_items;
	s->iface_methods = xcalloc(methods->nr_items, sizeof(struct ain_function_type));

	for (size_t i = 0; i < methods->nr_items; i++) {
		struct jaf_block_item *method = methods->items[i];
		assert(method->kind == JAF_DECL_FUN);
		s->iface_methods[i].name = conv_output(jaf_name_collapse(ain, &method->fun.name)->text);
		jaf_process_interface_method(ain, &s->iface_methods[i], &method->fun);
	}
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
			// XXX: If a function is defined and then later overridden, the jaf_fundecl
			//      object for the original definition needs to be updated to use the
			//      new function index generated by the override.
			if (block->items[i]->fun.super_no) {
				for (size_t j = 0; j < i; j++) {
					if (block->items[j]->kind != JAF_DECL_FUN)
						continue;
					if (block->items[j]->fun.func_no == block->items[i]->fun.func_no) {
						block->items[j]->fun.func_no = block->items[i]->fun.super_no;
					}
				}
			}
			break;
		case JAF_DECL_FUNCTYPE:
			jaf_process_functype(ain, &block->items[i]->fun);
			break;
		case JAF_DECL_DELEGATE:
			jaf_process_delegate(ain, &block->items[i]->fun);
			break;
		case JAF_DECL_STRUCT:
			jaf_process_structdef(ain, block->items[i]);
			break;
		case JAF_DECL_INTERFACE:
			jaf_process_interface(ain, block->items[i]);
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
	struct string *name = jaf_name_collapse(ain, &decl->name);
	f->name = xstrdup(name->text);
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
	int no = ain_add_library(ain, hll_name);
	struct ain_library *lib = &ain->libraries[no];
	lib->nr_functions = block->nr_items - 1; // -1 for EOF
	lib->functions = xcalloc(lib->nr_functions, sizeof(struct ain_hll_function));
	for (int i = 0; i < lib->nr_functions; i++) {
		if (block->items[i]->kind != JAF_DECL_FUN)
			JAF_ERROR(block->items[i],
				  "Only function declarations are allowed in HLL files: %d",
				  block->items[i]->kind);
		if (block->items[i]->fun.body)
			JAF_ERROR(block->items[i], "Function definitions not allowed in HLL files");
		_jaf_process_hll_declaration(ain, &block->items[i]->fun, &lib->functions[i]);
	}
}
