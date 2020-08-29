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
#include "ainedit.h"
#include "jaf.h"

static void jaf_resolve_types(struct ain *ain, struct jaf_block *block);
static void jaf_process_declarations(struct ain *ain, struct jaf_block *block);
static void jaf_analyze_block(struct jaf_env *env, struct jaf_block *block);

void jaf_define_struct(struct ain *ain, struct jaf_block_item *def)
{
	assert(def->kind == JAF_DECL_STRUCT);
	if (!def->struc.name)
		ERROR("Anonymous structs not supported");

	struct ain_struct s = {
		.name = encode_text(def->struc.name->text),
		.constructor = -1,
		.destructor = -1,
	};
	if (ain_get_struct(ain, s.name) >= 0)
		ERROR("Redefining structs not supported");
	def->struc.struct_no = ain_add_struct(ain, &s);
}

void jaf_define_functype(struct ain *ain, struct jaf_fundecl *decl)
{
	struct ain_function_type f = {0};
	f.name = encode_text(decl->name->text);
	if (ain_get_functype(ain, f.name) >= 0)
		ERROR("Multiple definitions of function type '%s'", decl->name->text);
	decl->func_no = ain_add_functype(ain, &f);
}

enum ain_data_type jaf_to_ain_data_type(enum jaf_type type, unsigned qualifiers)
{
	if (qualifiers & JAF_QUAL_REF && qualifiers & JAF_QUAL_ARRAY) {
		switch (type) {
		case JAF_VOID:     ERROR("void ref array type");
		case JAF_INT:      return AIN_REF_ARRAY_INT;
		case JAF_FLOAT:    return AIN_REF_ARRAY_FLOAT;
		case JAF_STRING:   return AIN_REF_ARRAY_STRING;
		case JAF_STRUCT:   return AIN_REF_ARRAY_STRUCT;
		case JAF_ENUM:     ERROR("Enums not supported");
		case JAF_TYPEDEF:  ERROR("Unresolved typedef");
		case JAF_FUNCTYPE: return AIN_REF_ARRAY_FUNC_TYPE;
		}
	} else if (qualifiers & JAF_QUAL_REF) {
		switch (type) {
		case JAF_VOID:     ERROR("void ref type");
		case JAF_INT:      return AIN_REF_INT;
		case JAF_FLOAT:    return AIN_REF_FLOAT;
		case JAF_STRING:   return AIN_REF_STRING;
		case JAF_STRUCT:   return AIN_REF_STRUCT;
		case JAF_ENUM:     ERROR("Enums not supported");
		case JAF_TYPEDEF:  ERROR("Unresolved typedef");
		case JAF_FUNCTYPE: return AIN_REF_FUNC_TYPE;
		}
	} else if (qualifiers & JAF_QUAL_ARRAY) {
		switch (type) {
		case JAF_VOID:     ERROR("void array type");
		case JAF_INT:      return AIN_ARRAY_INT;
		case JAF_FLOAT:    return AIN_ARRAY_FLOAT;
		case JAF_STRING:   return AIN_ARRAY_STRING;
		case JAF_STRUCT:   return AIN_ARRAY_STRUCT;
		case JAF_ENUM:     ERROR("Enums not supported");
		case JAF_TYPEDEF:  ERROR("Unresolved typedef");
		case JAF_FUNCTYPE: return AIN_ARRAY_FUNC_TYPE;
		}
	} else {
		switch (type) {
		case JAF_VOID:     return AIN_VOID;
		case JAF_INT:      return AIN_INT;
		case JAF_FLOAT:    return AIN_FLOAT;
		case JAF_STRING:   return AIN_STRING;
		case JAF_STRUCT:   return AIN_STRUCT;
		case JAF_ENUM:     ERROR("Enums not supported");
		case JAF_TYPEDEF:  ERROR("Unresolved typedef");
		case JAF_FUNCTYPE: return AIN_FUNC_TYPE;
		}
	}
	ERROR("Unknown type: %d", type);
}

static void jaf_to_ain_type(possibly_unused struct ain *ain, struct ain_type *out, struct jaf_type_specifier *in)
{
	out->data = jaf_to_ain_data_type(in->type, in->qualifiers);
	if (in->type == JAF_STRUCT || in->type == JAF_FUNCTYPE) {
		out->struc = in->struct_no;
	} else {
		out->struc = -1;
	}
	if (in->qualifiers & JAF_QUAL_ARRAY) {
		out->rank = in->rank;
	}
}

static void resolve_typedef(struct ain *ain, struct jaf_type_specifier *type)
{
	int no;
	char *u = encode_text(type->name->text);
	if ((no = ain_get_struct(ain, u)) >= 0) {
		type->type = JAF_STRUCT;
		type->struct_no = no;
	} else if ((no = ain_get_functype(ain, u)) >= 0) {
		type->type = JAF_FUNCTYPE;
		type->func_no = no;
	} else {
		ERROR("Failed to resolve typedef \"%s\"", type->name->text);
	}

	free(u);
}

static void jaf_to_initval(struct ain_initval *dst, struct jaf_expression *expr)
{
	switch (expr->type) {
	case JAF_EXP_INT:
		dst->data_type = AIN_INT;
		dst->int_value = expr->i;
		break;
	case JAF_EXP_FLOAT:
		dst->data_type = AIN_FLOAT;
		dst->float_value = expr->f;
		break;
	case JAF_EXP_STRING:
		dst->data_type = AIN_STRING;
		dst->string_value = strdup(expr->s->text);
		break;
	default:
		ERROR("Initval is not constant");
	}
}

static void analyze_expression(struct jaf_env *env, struct jaf_expression **expr)
{
	if (!*expr)
		return;
	jaf_derive_types(env, *expr);
	*expr = jaf_simplify(*expr);
}

static void analyze_block(struct jaf_env *env, struct jaf_block *block);

static void analyze_array_allocation(struct jaf_env *env, struct jaf_vardecl *decl)
{
	if (!decl->array_dims)
		return;
	for (size_t i = 0; i < decl->type->rank; i++) {
		analyze_expression(env, &decl->array_dims[i]);
		if (decl->array_dims[i]->type != JAF_EXP_INT)
			ERROR("Invalid expression as array size");
	}
}

static void analyze_global_declaration(struct jaf_env *env, struct jaf_vardecl *decl)
{
	if (!decl->init)
		return;
	analyze_expression(env, &decl->init);
	assert(decl->type);
	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);
	jaf_check_type(decl->init, &decl->valuetype);
	// add initval to ain object
	struct ain_initval init = { .global_index = decl->var_no };
	jaf_to_initval(&init, decl->init);
	ain_add_initval(env->ain, &init);
	analyze_array_allocation(env, decl);
	if (decl->init)
		analyze_expression(env, &decl->init);
}

static void analyze_local_declaration(struct jaf_env *env, struct jaf_vardecl *decl)
{
	assert(env->func_no >= 0 && env->func_no < env->ain->nr_functions);
	assert(decl->var_no >= 0 && decl->var_no < env->ain->functions[env->func_no].nr_vars);
	assert(decl->type);
	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);
	// add local to environment
	env->locals = xrealloc_array(env->locals, env->nr_locals, env->nr_locals+1, sizeof(struct ain_variable*));
	env->locals[env->nr_locals++] = &env->ain->functions[env->func_no].vars[decl->var_no];
	analyze_array_allocation(env, decl);
	if (decl->init)
		analyze_expression(env, &decl->init);
}

static void analyze_function(struct jaf_env *env, struct jaf_fundecl *decl)
{
	// create new scope with function arguments
	assert(decl->func_no < env->ain->nr_functions);
	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);
	struct ain_function *fun = &env->ain->functions[decl->func_no];
	struct jaf_env *funenv = xcalloc(1, sizeof(struct jaf_env));
	funenv->ain = env->ain;
	funenv->parent = env;
	funenv->func_no = decl->func_no;
	funenv->fundecl = decl;
	funenv->nr_locals = fun->nr_args;
	funenv->locals = xcalloc(funenv->nr_locals, sizeof(struct ain_variable*));
	for (size_t i = 0; i < funenv->nr_locals; i++) {
		funenv->locals[i] = &fun->vars[i];
	}

	jaf_analyze_block(funenv, decl->body);
	free(funenv->locals);
	free(funenv);
}

static void analyze_message(struct jaf_env *env, struct jaf_block_item *item)
{
	if (!item->msg.func) {
		item->msg.func_no = -1;
		return;
	}

	char *u = encode_text(item->msg.func->text);
	if ((item->msg.func_no = ain_get_function(env->ain, u)) < 0)
		ERROR("Undefined function: %s", item->msg.func->text);
	free(u);
}

static struct jaf_env *push_env(struct jaf_env *parent)
{
	struct jaf_env *newenv = xcalloc(1, sizeof(struct jaf_env));
	newenv->ain = parent->ain;
	newenv->parent = parent;
	newenv->func_no = parent->func_no;
	newenv->fundecl = parent->fundecl;
	return newenv;
}

static struct jaf_env *pop_env(struct jaf_env *env)
{
	struct jaf_env *parent = env->parent;
	free(env->locals);
	free(env);
	return parent;
}

static void analyze_statement(struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_env *blockenv;
	if (!item)
		return;
	switch (item->kind) {
	case JAF_DECL_VAR:
		if (env->parent)
			analyze_local_declaration(env, &item->var);
		else
			analyze_global_declaration(env, &item->var);
		break;
	case JAF_DECL_FUN:
		analyze_function(env, &item->fun);
		break;
	case JAF_STMT_LABELED:
		analyze_statement(env, item->label.stmt);
		break;
	case JAF_STMT_COMPOUND:
		analyze_block(env, item->block);
		break;
	case JAF_STMT_EXPRESSION:
		analyze_expression(env, &item->expr);
		break;
	case JAF_STMT_IF:
		analyze_expression(env, &item->cond.test);
		analyze_statement(env, item->cond.consequent);
		analyze_statement(env, item->cond.alternative);
		break;
	case JAF_STMT_SWITCH:
		analyze_expression(env, &item->swi.expr);
		analyze_block(env, item->swi.body);
		break;
	case JAF_STMT_WHILE:
	case JAF_STMT_DO_WHILE:
		analyze_expression(env, &item->while_loop.test);
		analyze_statement(env, item->while_loop.body);
		break;
	case JAF_STMT_FOR:
		blockenv = push_env(env);
		jaf_analyze_block(env, item->for_loop.init);
		analyze_expression(env, &item->for_loop.test);
		analyze_expression(env, &item->for_loop.after);
		analyze_statement(env, item->for_loop.body);
		pop_env(blockenv);
		break;
	case JAF_STMT_RETURN:
		analyze_expression(env, &item->expr);
		jaf_check_type(item->expr, &env->fundecl->valuetype);
		break;
	case JAF_STMT_CASE:
	case JAF_STMT_DEFAULT:
		analyze_statement(env, item->swi_case.stmt);
		break;
	case JAF_STMT_MESSAGE:
		analyze_message(env, item);
	case JAF_DECL_FUNCTYPE:
	case JAF_DECL_STRUCT:
	case JAF_STMT_GOTO:
	case JAF_STMT_CONTINUE:
	case JAF_STMT_BREAK:
	case JAF_EOF:
		break;
	}
}

static void jaf_analyze_block(struct jaf_env *env, struct jaf_block *block)
{
	for (size_t i = 0; i < block->nr_items; i++) {
		analyze_statement(env, block->items[i]);
	}
}

static void analyze_block(struct jaf_env *env, struct jaf_block *block)
{
	struct jaf_env *blockenv = push_env(env);
	jaf_analyze_block(blockenv, block);
	pop_env(blockenv);
}

static void resolve_structdef_types(struct ain *ain, struct jaf_block_item *item)
{
	assert(item->struc.struct_no >= 0);
	assert(item->struc.struct_no < ain->nr_structures);
	struct jaf_block *jaf_members = item->struc.members;
	struct ain_variable *members = xcalloc(jaf_members->nr_items, sizeof(struct ain_variable));
	for (size_t i = 0; i < item->struc.members->nr_items; i++) {
		if (jaf_members->items[i]->kind != JAF_DECL_VAR)
			continue;
		members[i].name = encode_text(jaf_members->items[i]->var.name->text);
		if (ain->version >= 12)
			members[i].name2 = strdup("");
		jaf_to_ain_type(ain, &members[i].type, jaf_members->items[i]->var.type);
	}
	struct ain_struct *s = &ain->structures[item->struc.struct_no];
	s->nr_members = jaf_members->nr_items;
	s->members = members;

}

static void resolve_statement_types(struct ain *ain, struct jaf_block_item *item)
{
	if (!item)
		return;
	switch (item->kind) {
	case JAF_DECL_VAR:
		if (item->var.type->type == JAF_TYPEDEF)
			resolve_typedef(ain, item->var.type);
		break;
	case JAF_DECL_FUNCTYPE:
		if (item->fun.params)
			jaf_resolve_types(ain, item->fun.params);
		break;
	case JAF_DECL_FUN:
		if (item->fun.params)
			jaf_resolve_types(ain, item->fun.params);
		jaf_resolve_types(ain, item->fun.body);
		break;
	case JAF_DECL_STRUCT:
		resolve_structdef_types(ain, item);
		break;
	case JAF_STMT_LABELED:
		resolve_statement_types(ain, item->label.stmt);
		break;
	case JAF_STMT_COMPOUND:
		jaf_resolve_types(ain, item->block);
		break;
	case JAF_STMT_IF:
		resolve_statement_types(ain, item->cond.consequent);
		resolve_statement_types(ain, item->cond.alternative);
		break;
	case JAF_STMT_SWITCH:
		jaf_resolve_types(ain, item->swi.body);
		break;
	case JAF_STMT_WHILE:
	case JAF_STMT_DO_WHILE:
		resolve_statement_types(ain, item->while_loop.body);
		break;
	case JAF_STMT_FOR:
		jaf_resolve_types(ain, item->for_loop.init);
		resolve_statement_types(ain, item->for_loop.body);
		break;
	case JAF_STMT_CASE:
	case JAF_STMT_DEFAULT:
		resolve_statement_types(ain, item->swi_case.stmt);
		break;
	case JAF_STMT_EXPRESSION:
	case JAF_STMT_GOTO:
	case JAF_STMT_CONTINUE:
	case JAF_STMT_BREAK:
	case JAF_STMT_RETURN:
	case JAF_STMT_MESSAGE:
	case JAF_EOF:
		break;
	}
}

static void jaf_resolve_types(struct ain *ain, struct jaf_block *block)
{
	for (size_t i = 0; i < block->nr_items; i++) {
		resolve_statement_types(ain, block->items[i]);
	}
}

static void init_variable(struct ain *ain, struct ain_variable *vars, int *var_no, struct jaf_vardecl *decl)
{
	vars[*var_no].name = encode_text(decl->name->text);
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

static struct ain_variable *block_get_vars(struct ain *ain, struct jaf_block *block, struct ain_variable *vars, int *nr_vars);

static struct ain_variable *block_item_get_vars(struct ain *ain, struct jaf_block_item *item, struct ain_variable *vars, int *nr_vars)
{
	if (!item)
		return vars;
	switch (item->kind) {
	case JAF_DECL_VAR:
		vars = xrealloc_array(vars, *nr_vars, *nr_vars + 2, sizeof(struct ain_variable));
		init_variable(ain, vars, nr_vars, &item->var);
		break;
	case JAF_STMT_LABELED:
		vars = block_item_get_vars(ain, item->label.stmt, vars, nr_vars);
		break;
	case JAF_STMT_COMPOUND:
		vars = block_get_vars(ain, item->block, vars, nr_vars);
		break;
	case JAF_STMT_IF:
		vars = block_item_get_vars(ain, item->cond.consequent, vars, nr_vars);
		vars = block_item_get_vars(ain, item->cond.alternative, vars, nr_vars);
		break;
	case JAF_STMT_SWITCH:
		vars = block_get_vars(ain, item->swi.body, vars, nr_vars);
		break;
	case JAF_STMT_WHILE:
	case JAF_STMT_DO_WHILE:
		vars = block_item_get_vars(ain, item->while_loop.body, vars, nr_vars);
		break;
	case JAF_STMT_FOR:
		vars = block_get_vars(ain, item->for_loop.init, vars, nr_vars);
		vars = block_item_get_vars(ain, item->for_loop.body, vars, nr_vars);
		break;
	case JAF_STMT_CASE:
	case JAF_STMT_DEFAULT:
		vars = block_item_get_vars(ain, item->swi_case.stmt, vars, nr_vars);
		break;

	case JAF_DECL_FUNCTYPE:
	case JAF_DECL_STRUCT:
	case JAF_STMT_EXPRESSION:
	case JAF_STMT_GOTO:
	case JAF_STMT_CONTINUE:
	case JAF_STMT_BREAK:
	case JAF_STMT_RETURN:
	case JAF_STMT_MESSAGE:
	case JAF_EOF:
		break;

	case JAF_DECL_FUN:
		ERROR("Nested functions not supported");
	}
	return vars;
}

static struct ain_variable *block_get_vars(struct ain *ain, struct jaf_block *block, struct ain_variable *vars, int *nr_vars)
{
	for (size_t i = 0; i < block->nr_items; i++) {
		vars = block_item_get_vars(ain, block->items[i], vars, nr_vars);
	}
	return vars;
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

static void add_function(struct ain *ain, struct jaf_fundecl *decl)
{
	struct ain_function f = {0};
	f.name = strdup(decl->name->text);
	jaf_to_ain_type(ain, &f.return_type, decl->type);
	function_init_vars(ain, decl, &f.nr_args, &f.nr_vars, &f.vars);

	decl->func_no = ain_add_function(ain, &f);
	if (!strcmp(decl->name->text, "main")) {
		if (decl->params || decl->type->type != JAF_INT)
			ERROR("Invalid signature for main function");
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
			ERROR("Invalid signature for message function");
		}
		if (ain->msgf > 0)
			WARNING("Overriding message function");
		ain->msgf = decl->func_no;
	}
}

static void add_functype(struct ain *ain, struct jaf_fundecl *decl)
{
	assert(decl->func_no >= 0 && decl->func_no <= ain->nr_function_types);
	struct ain_function_type *f = &ain->function_types[decl->func_no];
	jaf_to_ain_type(ain, &f->return_type, decl->type);
	function_init_vars(ain, decl, &f->nr_arguments, &f->nr_variables, &f->variables);
}

static void add_global(struct ain *ain, struct jaf_vardecl *decl)
{
	struct ain_variable v = {0};
	v.name = encode_text(decl->name->text);
	jaf_to_ain_type(ain, &v.type, decl->type);
	ain_add_global(ain, &v);
}

static void jaf_process_declarations(struct ain *ain, struct jaf_block *block)
{
	for (size_t i = 0; i < block->nr_items; i++) {
		switch (block->items[i]->kind) {
		case JAF_DECL_VAR:
			add_global(ain, &block->items[i]->var);
			break;
		case JAF_DECL_FUN:
			add_function(ain, &block->items[i]->fun);
			break;
		case JAF_DECL_FUNCTYPE:
			add_functype(ain, &block->items[i]->fun);
			break;
		case JAF_DECL_STRUCT:
		case JAF_EOF:
			break;
		default:
			ERROR("Unhandled declaration at top-level: %d", block->items[i]->kind);
		}
	}
}

/*
 * This function is responsible for registering names/definitions into the ain file.
 * It should be called before the static analysis phase.
 */
void jaf_resolve_declarations(struct ain *ain, struct jaf_block *block)
{
	// pass 1: typedefs && struct definitions
	jaf_resolve_types(ain, block);
	// pass 2: register globals (names, types)
	jaf_process_declarations(ain, block);
}

static void jaf_process_hll_declaration(struct ain *ain, struct jaf_fundecl *decl, struct ain_hll_function *f)
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

void jaf_resolve_hll_declarations(struct ain *ain, struct jaf_block *block, const char *hll_name)
{
	struct ain_library lib = {0};
	lib.name = xstrdup(hll_name);
	lib.nr_functions = block->nr_items - 1; // -1 for EOF
	lib.functions = xcalloc(lib.nr_functions, sizeof(struct ain_hll_function));
	for (int i = 0; i < lib.nr_functions; i++) {
		if (block->items[i]->kind != JAF_DECL_FUN)
			ERROR("Only function declarations are allowed in HLL files: %d", block->items[i]->kind);
		if (block->items[i]->fun.body)
			ERROR("Function definitions not allowed in HLL files");
		jaf_process_hll_declaration(ain, &block->items[i]->fun, &lib.functions[i]);
	}
	ain_add_library(ain, &lib);
}

struct jaf_block *jaf_static_analyze(struct ain *ain, struct jaf_block *block)
{
	struct jaf_env env = {
		.ain = ain,
		.parent = NULL
	};

	// pass 3: type analysis & simplification & global initvals
	jaf_analyze_block(&env, block);

	return block;
}
