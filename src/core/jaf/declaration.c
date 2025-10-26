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

static jaf_var_set var_set_diff(const jaf_var_set *a, const jaf_var_set *b)
{
	// TODO? Using O(n^2) algorithm here, but not sure if it matters in practice
	jaf_var_set diff = {0};
	struct jaf_vardecl *p, *q;
	kv_foreach(p, *a) {
		kv_foreach(q, *b) {
			if (p == q)
				goto skip;
		}
		// p is not in set b
		kv_push(struct jaf_vardecl*, diff, p);
skip:
	}
	return diff;
}

enum scope_type {
	SCOPE_ANON,
	SCOPE_LOOP,
	SCOPE_SWITCH,
};

struct jump {
	struct jaf_block_item *stmt;
	jaf_var_set live_vars;
};

typedef kvec_t(struct jump) jump_list;

static void jump_list_destroy(jump_list *list)
{
	struct jump *p;
	kv_foreach_p(p, *list) {
		kv_destroy(p->live_vars);
	}
	kv_destroy(*list);
}

struct scope {
	struct scope *parent;
	jaf_var_set initial_vars;
	jump_list breaks;
	jump_list continues;
};

static struct scope *scope_push(struct scope *scope)
{
	struct scope *head = xcalloc(1, sizeof(struct scope));
	head->parent = scope;
	return head;
}

static struct scope *scope_pop(struct scope *scope)
{
	struct scope *head = scope->parent;
	kv_destroy(scope->initial_vars);
	assert(kv_size(scope->breaks) == 0);
	assert(kv_size(scope->continues) == 0);
	jump_list_destroy(&scope->breaks);
	jump_list_destroy(&scope->continues);
	free(scope);
	return head;
}

struct label {
	struct string *name;
	bool resolved;
	jaf_var_set live_vars;
	kvec_t(unsigned) gotos;
};

struct alloc_state {
	struct ain *ain;
	struct ain_variable *vars;
	int nr_vars;
	struct scope *scope;
	jump_list gotos;
	kvec_t(struct label) labels;
};

static warn_unused int _init_variable(struct alloc_state *state, const char *name,
		struct ain_type *type)
{
	int var = state->nr_vars;
	state->vars[var].name = conv_output(name);
	if (state->ain->version >= 12)
		state->vars[var].name2 = strdup("");
	state->vars[var].type = *type;

	// immediate reference types need extra slot (page+index)
	switch (state->vars[var].type.data) {
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
	case AIN_IFACE:
		state->nr_vars++;
		state->vars[var+1].name = strdup("<void>");
		if (state->ain->version >= 12)
			state->vars[var+1].name2 = strdup("");
		state->vars[var+1].type.data = AIN_VOID;
		state->vars[var+1].type.struc = -1;
		break;
	default:
		break;
	}

	state->nr_vars++;
	return var;
}

static warn_unused int init_variable(struct alloc_state *state, const char *name,
		struct jaf_type_specifier *type)
{
	struct ain_type atype;
	jaf_to_ain_type(state->ain, &atype, type);
	return _init_variable(state, name, &atype);
}

static void env_to_var_list(struct jaf_env *env, jaf_var_set *out)
{
	for (size_t i = 0; i < env->nr_locals; i++) {
		struct jaf_env_local *local = &env->locals[i];
		if (local->is_const)
			continue;
		kv_push(struct jaf_vardecl*, *out, local->decl);
	}

	if (env->parent)
		env_to_var_list(env->parent, out);
}

static void start_scope(struct alloc_state *state, struct jaf_env *env)
{
	state->scope = scope_push(state->scope);
	env_to_var_list(env, &state->scope->initial_vars);
}

static void update_jumps(jump_list *jumps, struct scope *scope)
{
	struct jump *j;
	kv_foreach_p(j, *jumps) {
		j->stmt->delete_vars = var_set_diff(&j->live_vars, &scope->initial_vars);
		kv_destroy(j->live_vars);
	}
	kv_destroy(*jumps);
	kv_init(*jumps);
}

static void carry_continues(struct scope *scope)
{
	if (!scope->parent && kv_size(scope->continues) > 0)
		COMPILER_ERROR(kv_A(scope->continues, 0).stmt, "Unresolved continue statement");

	struct jump *j;
	kv_foreach_p(j, scope->continues) {
		kv_push(struct jump, scope->parent->continues, *j);
	}
	kv_destroy(scope->continues);
	kv_init(scope->continues);
}

static void carry_breaks(struct scope *scope)
{
	if (!scope->parent && kv_size(scope->breaks) > 0)
		COMPILER_ERROR(kv_A(scope->breaks, 0).stmt, "Unresolved break statement");

	struct jump *j;
	kv_foreach_p(j, scope->breaks) {
		kv_push(struct jump, scope->parent->breaks, *j);
	}
	kv_destroy(scope->breaks);
	kv_init(scope->breaks);
}

static void end_scope(struct alloc_state *state, enum scope_type type)
{
	switch (type) {
	case SCOPE_LOOP:
		update_jumps(&state->scope->breaks, state->scope);
		update_jumps(&state->scope->continues, state->scope);
		break;
	case SCOPE_SWITCH:
		update_jumps(&state->scope->breaks, state->scope);
		carry_continues(state->scope);
		break;
	case SCOPE_ANON:
		carry_breaks(state->scope);
		carry_continues(state->scope);
		break;
	}
	state->scope = scope_pop(state->scope);
}

static struct label *find_label(struct alloc_state *state, const char *name)
{
	struct label *p;
	kv_foreach_p(p, state->labels) {
		if (!strcmp(p->name->text, name))
			return p;
	}
	return NULL;
}

static void add_label(struct alloc_state *state, struct jaf_env *env,
		struct jaf_block_item *stmt)
{
	struct label *p = find_label(state, stmt->label.name->text);
	if (p) {
		if (p->resolved)
			JAF_ERROR(stmt, "Duplicate label");
		p->resolved = true;
		env_to_var_list(env, &p->live_vars);
		return;
	}

	p = kv_pushp(struct label, state->labels);
	p->name = string_dup(stmt->label.name);
	p->resolved = true;
	kv_init(p->gotos);
	kv_init(p->live_vars);
	env_to_var_list(env, &p->live_vars);
}

static void jump_list_add(jump_list *list, struct jaf_env *env, struct jaf_block_item *stmt)
{
	struct jump *p = kv_pushp(struct jump, *list);
	p->stmt = stmt;
	kv_init(p->live_vars);
	env_to_var_list(env, &p->live_vars);
}

static void add_goto(struct alloc_state *state, struct jaf_env *env,
		struct jaf_block_item *stmt)
{
	unsigned no = kv_size(state->gotos);
	jump_list_add(&state->gotos, env, stmt);

	struct label *p = find_label(state, stmt->target->text);
	if (!p) {
		p = kv_pushp(struct label, state->labels);
		p->name = string_dup(stmt->target);
		p->resolved = false;
		kv_init(p->gotos);
		kv_init(p->live_vars);
	}
	kv_push(unsigned, p->gotos, no);
}

static void stmt_get_vars_pre(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	struct alloc_state *state = visitor->data;
	switch (stmt->kind) {
	case JAF_STMT_COMPOUND:
	case JAF_STMT_WHILE:
	case JAF_STMT_DO_WHILE:
	case JAF_STMT_FOR:
		start_scope(state, visitor->env);
		break;
	case JAF_STMT_LABELED:
		add_label(state, visitor->env, stmt);
		break;
	case JAF_STMT_GOTO:
		add_goto(state, visitor->env, stmt);
		break;
	case JAF_STMT_CONTINUE:
		jump_list_add(&state->scope->continues, visitor->env, stmt);
		break;
	case JAF_STMT_BREAK:
		jump_list_add(&state->scope->breaks, visitor->env, stmt);
		break;
	default:
		break;
	}
}

static void stmt_get_vars_post(struct jaf_block_item *stmt, struct jaf_visitor *visitor)
{
	struct alloc_state *state = visitor->data;
	switch (stmt->kind) {
	case JAF_DECL_VAR:
		if (!(stmt->var.type->qualifiers & JAF_QUAL_CONST)) {
			state->vars = xrealloc_array(state->vars, state->nr_vars, state->nr_vars+2,
					sizeof(struct ain_variable));
			stmt->var.var = init_variable(state, stmt->var.name->text, stmt->var.type);
		}
		break;
	case JAF_DECL_FUN:
		JAF_ERROR(stmt, "Nested Functions not supported");
	case JAF_STMT_COMPOUND:
		end_scope(state, SCOPE_ANON);
		break;
	case JAF_STMT_WHILE:
	case JAF_STMT_DO_WHILE:
	case JAF_STMT_FOR:
		end_scope(state, SCOPE_LOOP);
		break;
	case JAF_STMT_SWITCH:
		end_scope(state, SCOPE_SWITCH);
		break;
	default:
		break;
	}
}

static int create_dummy_var(struct alloc_state *state, struct ain_type *type, const char *fmt, ...)
{
	char name[1024];
	char dfmt[1024];

	snprintf(dfmt, 1023, "<dummy : %s>", fmt);

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(name, 1023, dfmt, ap);
	va_end(ap);

	state->vars = xrealloc_array(state->vars, state->nr_vars, state->nr_vars+2,
			sizeof(struct ain_variable));
	struct ain_type copy;
	ain_copy_type(&copy, type);
	return _init_variable(state, name, &copy);
}

static struct jaf_expression *create_dummyref(struct jaf_expression *expr, int var_no)
{
	struct jaf_expression *dummy = jaf_expr(JAF_EXP_DUMMYREF, 0);
	dummy->dummy.expr = expr;
	dummy->dummy.var_no = var_no;
	ain_copy_type(&dummy->valuetype, &expr->valuetype);
	return dummy;
}

static struct jaf_expression *expr_get_vars(struct jaf_expression *expr, struct jaf_visitor *visitor)
{
	struct alloc_state *state = visitor->data;
	switch (expr->type) {
	case JAF_EXP_NEW: {
		assert(expr->new.type->name);
		struct ain_type t;
		jaf_to_ain_type(state->ain, &t, expr->new.type);
		int var_no = create_dummy_var(state, &t, "new %s", expr->new.type->name->text);
		expr = create_dummyref(expr, var_no);
		ain_free_type(&t);
		break;
	}
	case JAF_EXP_FUNCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_INTERFACE_CALL:
	case JAF_EXP_SUPER_CALL:
	case JAF_EXP_HLLCALL: {
		if (!ain_is_ref_data_type(expr->valuetype.data))
			break;
		int var_no = create_dummy_var(state, &expr->valuetype, "return");
		expr = create_dummyref(expr, var_no);
		break;
	}
	default:
		break;
	}

	return expr;
}

static void resolve_gotos(struct alloc_state *state)
{
	struct label *p;
	kv_foreach_p(p, state->labels) {
		if (!p->resolved) {
			assert(kv_size(p->gotos) > 0);
			unsigned i = kv_A(p->gotos, 0);
			assert(i < kv_size(state->gotos));
			JAF_ERROR(kv_A(state->gotos, i).stmt, "Unresolved label");
		}
		unsigned i;
		kv_foreach(i, p->gotos) {
			assert(i < kv_size(state->gotos));
			struct jump *j = &kv_A(state->gotos, i);
			j->stmt->delete_vars = var_set_diff(&j->live_vars, &p->live_vars);
		}
	}
}

static struct ain_variable *function_get_vars(struct ain *ain, struct jaf_block *block,
		struct ain_variable *vars, int *nr_vars)
{
	struct alloc_state state = {
		.ain = ain,
		.vars = vars,
		.nr_vars = *nr_vars,
	};
	struct jaf_visitor visitor = {
		.visit_stmt_pre = stmt_get_vars_pre,
		.visit_stmt_post = stmt_get_vars_post,
		.visit_expr_post = expr_get_vars,
		.data = &state,
	};

	jaf_accept_block(ain, block, &visitor);

	resolve_gotos(&state);

	struct label *p;
	kv_foreach_p(p, state.labels) {
		free_string(p->name);
		kv_destroy(p->gotos);
	}
	kv_destroy(state.labels);

	struct jump *j;
	kv_foreach_p(j, state.gotos) {
		kv_destroy(j->live_vars);
	}
	kv_destroy(state.gotos);

	*nr_vars = state.nr_vars;
	return state.vars;
}

void jaf_allocate_variables(struct ain *ain, struct jaf_block *block)
{
	for (size_t i = 0; i < block->nr_items; i++) {
		struct jaf_block_item *item = block->items[i];
		if (item->kind == JAF_DECL_FUN && item->fun.body) {
			assert(item->fun.func_no > 0 && item->fun.func_no <= ain->nr_functions);
			struct ain_function *f = &ain->functions[item->fun.func_no];
			f->vars = function_get_vars(ain, item->fun.body, f->vars, &f->nr_vars);
		} else if (item->kind == JAF_DECL_STRUCT) {
			jaf_allocate_variables(ain, item->struc.methods);
		}
	}
}

static void function_init_args(struct ain *ain, struct jaf_fundecl *decl, int32_t *nr_args,
		int32_t *nr_vars, struct ain_variable **vars)
{
	int nr_params = decl->params ? decl->params->nr_items : 0;
	*nr_args = 0;
	*vars = xcalloc(nr_params * 2, sizeof(struct ain_variable));
	for (int i = 0; i < nr_params; i++) {
		assert(decl->params->items[i]->kind == JAF_DECL_VAR);
		assert(decl->params->items[i]->var.name);
		struct jaf_vardecl *param = &decl->params->items[i]->var;
		struct alloc_state state = {
			.ain = ain,
			.vars = *vars,
			.nr_vars = *nr_args
		};
		param->var = init_variable(&state, param->name->text, param->type);
		*nr_args = state.nr_vars;
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
	function_init_args(ain, decl, &fun->nr_args, &fun->nr_vars, &fun->vars);
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
		function_init_args(ain, decl, &f.nr_args, &f.nr_vars, &f.vars);
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
		function_init_args(ain, decl, &f->nr_args, &f->nr_vars, &f->vars);
		f->struct_type = decl->name.struct_no;
		decl->super_no = 0;
		if (decl->name.is_constructor || decl->name.is_destructor) {
			assert(decl->name.struct_no >= 0 && decl->name.struct_no < ain->nr_structures);
			struct ain_struct *s = &ain->structures[decl->name.struct_no];
			if (decl->name.is_constructor) {
				s->constructor = decl->func_no;
				decl->fun_type = JAF_FUN_CONSTRUCTOR;
			} else {
				s->destructor = decl->func_no;
				decl->fun_type = JAF_FUN_DESTRUCTOR;
			}
		} else if (decl->name.struct_no >= 0) {
			decl->fun_type = JAF_FUN_METHOD;
		} else {
			decl->fun_type = JAF_FUN_PROCEDURE;
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
	function_init_args(ain, decl, &f->nr_arguments, &f->nr_variables, &f->variables);
}

static void jaf_process_delegate(struct ain *ain, struct jaf_fundecl *decl)
{
	assert(decl->func_no >= 0 && decl->func_no <= ain->nr_delegates);
	struct ain_function_type *f = &ain->delegates[decl->func_no];
	jaf_to_ain_type(ain, &f->return_type, decl->type);
	function_init_args(ain, decl, &f->nr_arguments, &f->nr_variables, &f->variables);
}

static void jaf_process_global(struct ain *ain, struct jaf_vardecl *decl)
{
	char *tmp = conv_output(decl->name->text);
	decl->var = ain_add_global(ain, tmp);
	free(tmp);

	jaf_to_ain_type(ain, &ain->globals[decl->var].type, decl->type);
}

static jaf_var_set block_get_array_allocs(struct jaf_block *block)
{
	jaf_var_set vars = {0};
	for (unsigned i = 0; i < block->nr_items; i++) {
		if (block->items[i]->kind == JAF_DECL_VAR && block->items[i]->var.array_dims) {
			kv_push(struct jaf_vardecl*, vars, &block->items[i]->var);
		}
	}
	return vars;
}

static int struct_nr_iface_methods(struct ain *ain, struct ain_struct *s)
{
	int count = 0;
	for (int i = 0; i < s->nr_interfaces; i++) {
		int iface_no = s->interfaces[i].struct_type;
		assert(iface_no >= 0 && iface_no < ain->nr_structures);
		count += ain->structures[iface_no].nr_iface_methods;
	}
	return count;
}

static int get_struct_method_no(struct ain *ain, struct ain_struct *impl,
		const char *method_name)
{
	struct string *name = make_string(impl->name, strlen(impl->name));
	string_push_back(&name, '@');
	string_append_cstr(&name, method_name, strlen(method_name));
	int mno = ain_get_function(ain, name->text);
	free_string(name);
	if (mno <= 0)
		_COMPILER_ERROR(NULL, -1, "Undefined method: %s", name->text);
	return mno;
}

static struct jaf_block_item *array_int_assign_stmt(struct string *name, int i, int v)
{
	struct jaf_expression *obj = jaf_member_expr(jaf_this(), string_dup(name));
	struct jaf_expression *sub = jaf_subscript_expr(obj, jaf_integer(i));
	struct jaf_expression *assign = jaf_binary_expr(JAF_ASSIGN, sub, jaf_integer(v));
	return jaf_expression_statement(assign);
}

static struct jaf_expression *var_expr(struct string *name, bool global)
{
	if (global)
		return jaf_identifier(string_dup(name));
	return jaf_member_expr(jaf_this(), string_dup(name));
}

static struct jaf_block_item *array_alloc_stmt_v11(struct ain *ain, struct jaf_vardecl *decl,
		bool global)
{
	if (decl->type->rank > 4)
		_JAF_ERROR(NULL, -1, "Invalid array rank: %u", decl->type->rank);

	struct jaf_expression *var = var_expr(decl->name, global);
	struct jaf_expression *func = jaf_member_expr(var, make_string("Alloc", 5));

	struct jaf_argument_list *dims = NULL;
	for (unsigned i = 0; i < decl->type->rank; i++) {
		dims = jaf_args(dims, jaf_copy_expression(decl->array_dims[i]));
	}
	for (unsigned i = decl->type->rank; i < 4; i++) {
		dims = jaf_args(dims, jaf_integer(-1));
	}
	return jaf_expression_statement(jaf_function_call(func, dims));
}

static struct jaf_block_item *array_alloc_stmt(struct ain *ain, struct jaf_vardecl *decl,
		bool global)
{
	if (AIN_VERSION_GTE(ain, 11, 0))
		return array_alloc_stmt_v11(ain, decl, global);

	struct jaf_expression *var = var_expr(decl->name, global);
	struct jaf_expression *func = jaf_member_expr(var, make_string("Alloc", 5));

	struct jaf_argument_list *dims = NULL;
	for (unsigned i = 0; i < decl->type->rank; i++) {
		dims = jaf_args(dims, jaf_copy_expression(decl->array_dims[i]));
	}

	return jaf_expression_statement(jaf_function_call(func, dims));
}

static struct jaf_block *vtable_initializer(struct ain *ain, struct ain_struct *s)
{
	struct string *vtable = make_string("<vtable>", 8);
	int nr_iface_methods = struct_nr_iface_methods(ain, s);
	struct jaf_expression *var = jaf_member_expr(jaf_this(), vtable);
	struct jaf_expression *func = jaf_member_expr(var, make_string("Alloc", 5));
	struct jaf_argument_list *args = jaf_args(NULL, jaf_integer(nr_iface_methods));
	args = jaf_args(args, jaf_integer(-1));
	args = jaf_args(args, jaf_integer(-1));
	args = jaf_args(args, jaf_integer(-1));
	struct jaf_expression *call = jaf_function_call(func, args);
	struct jaf_block *block = jaf_block(jaf_expression_statement(call));

	for (int i = 0, vno = 0; i < s->nr_interfaces; i++) {
		int iface_no = s->interfaces[i].struct_type;
		assert(iface_no >= 0 && iface_no < ain->nr_structures);
		struct ain_struct *iface = &ain->structures[s->interfaces[i].struct_type];
		for (int m = 0; m < iface->nr_iface_methods; m++, vno++) {
			int mno = get_struct_method_no(ain, s, iface->iface_methods[m].name);
			block = jaf_block_append(block, array_int_assign_stmt(vtable, vno, mno));
		}
	}
	return block;
}

static struct jaf_block *array_initializers(struct ain *ain, jaf_var_set *vars, bool global)
{
	struct jaf_block *block = jaf_block(NULL);
	struct jaf_vardecl *v;
	kv_foreach(v, *vars) {
		block = jaf_block_append(block, array_alloc_stmt(ain, v, global));
	}

	return block;
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

static void jaf_process_structdef(struct ain *ain, struct jaf_block_item *item)
{
	assert(item->struc.struct_no >= 0);
	assert(item->struc.struct_no < ain->nr_structures);
	struct ain_struct *s = &ain->structures[item->struc.struct_no];

	struct jaf_block *jaf_members = item->struc.members;
	bool need_vtable = kv_size(item->struc.interfaces) > 0;
	int nr_members = jaf_members->nr_items + (need_vtable ? 1 : 0);
	struct ain_variable *members = xcalloc(nr_members, sizeof(struct ain_variable));

	// add <vtable> member if necessary
	unsigned m = 0;
	if (need_vtable) {
		members[0].name = xstrdup("<vtable>");
		if (AIN_VERSION_GTE(ain, 12, 0))
			members[0].name2 = xstrdup("<vtable>");
		members[0].type.data = AIN_ARRAY;
		members[0].type.rank = 1;
		members[0].type.array_type = xcalloc(1, sizeof(struct ain_type));
		members[0].type.array_type->data = AIN_INT;
		m++;
	}

	bool need_alloc = need_vtable;

	// process members
	for (unsigned i = 0; i < jaf_members->nr_items; i++, m++) {
		assert(jaf_members->items[i]->kind == JAF_DECL_VAR);
		members[m].name = conv_output(jaf_members->items[i]->var.name->text);
		if (ain->version >= 12)
			members[m].name2 = strdup("");
		jaf_to_ain_type(ain, &members[m].type, jaf_members->items[i]->var.type);
		if (jaf_members->items[i]->var.array_dims)
			need_alloc = true;
	}

	// process methods
	struct jaf_block_item *ctor = NULL;
	for (unsigned i = 0; i < item->struc.methods->nr_items; i++) {
		struct jaf_block_item *method = item->struc.methods->items[i];
		assert(method->kind == JAF_DECL_FUN);
		jaf_process_function(ain, method);
		if (method->fun.fun_type == JAF_FUN_CONSTRUCTOR)
			ctor = method;
	}
	s->nr_members = nr_members;
	s->members = members;

	// process interfaces
	s->nr_interfaces = kv_size(item->struc.interfaces);
	s->interfaces = xcalloc(s->nr_interfaces, sizeof(struct ain_interface));

	int iface_index = 0;
	int vtable_offset = 0;
	struct string *p;
	kv_foreach(p, item->struc.interfaces) {
		char *name = conv_output(p->text);
		int iface_no = ain_get_struct(ain, name);
		free(name);
		if (iface_no < 0)
			JAF_ERROR(item, "Undefined interface: %s", p->text);
		struct ain_struct *iface = &ain->structures[iface_no];
		if (!iface->is_interface)
			JAF_ERROR(item, "Not an interface: %s", p->text);

		// write interface data to struct
		s->interfaces[iface_index].struct_type = iface_no;
		s->interfaces[iface_index].vtable_offset = vtable_offset;

		for (int i = 0; i < iface->nr_iface_methods; i++) {
			// check that method with correct signature exists for struct
			int mno = check_iface_method(ain, &iface->iface_methods[i], s);
			if (mno < 0)
				JAF_ERROR(item, "Interface method not implemented: %s::%s",
						item->struc.name->text, p->text);
		}

		iface_index++;
		vtable_offset += iface->nr_iface_methods;
	}

	if (need_alloc) {
		// generate initializer function
		jaf_var_set vars = block_get_array_allocs(item->struc.members);
		struct jaf_block *body = array_initializers(ain, &vars, false);
		kv_destroy(vars);
		if (need_vtable) {
			struct jaf_block *vtable = vtable_initializer(ain, s);
			body = jaf_merge_blocks(vtable, body);
		}

		struct string *fname;
		if (ctor) {
			// name of fun is struct::2
			fname = make_string("2", 1);
		} else {
			// name of fun is struct::struct (translated to struct::0 elsewhere)
			fname = string_dup(item->struc.name);
		}

		// add alloc fun to method list
		struct jaf_name mname;
		jaf_name_init(&mname, string_dup(item->struc.name));
		jaf_name_append(&mname, fname);
		struct jaf_block_item *alloc_fun = _jaf_function(NULL, &mname, NULL, body);
		item->struc.methods = jaf_block_append(item->struc.methods, alloc_fun);

		// process
		jaf_process_function(ain, alloc_fun);
	}
}

static void jaf_process_interface_method(struct ain *ain, struct ain_function_type *ft,
		struct jaf_fundecl *decl)
{
	jaf_to_ain_type(ain, &ft->return_type, decl->type);
	function_init_args(ain, decl, &ft->nr_arguments, &ft->nr_variables, &ft->variables);
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

static void jaf_process_global_allocs(struct ain *ain, struct jaf_block *block)
{
	// TODO: in v12+, GSET section is not present and we have to initialize
	//       all globals here
	jaf_var_set allocs = block_get_array_allocs(block);
	if (kv_size(allocs) == 0)
		return;

	struct jaf_name name;
	jaf_name_init(&name, make_string("0", 1));
	struct jaf_type_specifier *type = jaf_type(JAF_VOID);
	struct jaf_block *body = jaf_block(NULL);

	// if "0" function already exists, override it
	int alloc_no = ain_get_function(ain, "0");
	if (alloc_no > 0) {
		// add override qualifier
		type->qualifiers = JAF_QUAL_OVERRIDE;
		// begin body with call to super()
		struct jaf_expression *super = jaf_identifier(make_string("super", 5));
		struct jaf_expression *call = jaf_function_call(super, NULL);
		body = jaf_block_append(body, jaf_expression_statement(call));
	}

	// generate array initializers
	body = jaf_merge_blocks(body, array_initializers(ain, &allocs, true));
	// add function to toplevel
	struct jaf_block_item *alloc_fun = _jaf_function(type, &name, NULL, body);
	jaf_block_append(block, alloc_fun);
	// process function
	jaf_process_function(ain, alloc_fun);

	kv_destroy(allocs);
}

void jaf_process_declarations(struct ain *ain, struct jaf_block *block)
{
	// process interfaces first so that method list is availble when
	// processing structs that implement interfaces
	for (size_t i = 0; i < block->nr_items; i++) {
		if (block->items[i]->kind == JAF_DECL_INTERFACE)
			jaf_process_interface(ain, block->items[i]);
	}

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
					struct jaf_block_item *item_i = block->items[i];
					struct jaf_block_item *item_j = block->items[j];
					if (item_j->kind != JAF_DECL_FUN)
						continue;
					if (item_j->fun.func_no == item_i->fun.func_no) {
						item_j->fun.func_no = item_i->fun.super_no;
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
		case JAF_EOF:
			break;
		default:
			JAF_ERROR(block->items[i], "Unhandled declaration at top-level: %d",
				  block->items[i]->kind);
		}
	}

	jaf_process_global_allocs(ain, block);
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
