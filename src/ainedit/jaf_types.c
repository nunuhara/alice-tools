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
#include "ainedit.h"
#include "jaf.h"

#define AIN_FUNCTION 8000

// TODO: better error messages
#define TYPE_ERROR(expr, expected) JAF_ERROR(expr, "Type error (expected %s; got %s)", strdup(ain_strtype(NULL, expected, -1)), strdup(ain_strtype(NULL, expr->valuetype.data, -1)))
#define TYPE_CHECK(expr, expected) { if (expr->valuetype.data != expected) TYPE_ERROR(expr, expected); }

const char *jaf_typestr(enum jaf_type type)
{
	switch (type) {
	case JAF_VOID:     return "void";
	case JAF_INT:      return "int";
	case JAF_FLOAT:    return "float";
	case JAF_STRING:   return "string";
	case JAF_STRUCT:   return "struct";
	case JAF_ENUM:     return "enum";
	case JAF_ARRAY:    return "array";
	case JAF_WRAP:     return "wrap";
	case JAF_TYPEDEF:  return "typedef";
	case JAF_FUNCTYPE: return "functype";
	}
	return "unknown";
}

static enum ain_data_type strip_ref(enum ain_data_type type)
{
	switch (type) {
	case AIN_REF_INT:             return AIN_INT;
	case AIN_REF_FLOAT:           return AIN_FLOAT;
	case AIN_REF_STRING:          return AIN_STRING;
	case AIN_REF_STRUCT:          return AIN_STRUCT;
	case AIN_REF_ARRAY_INT:       return AIN_ARRAY_INT;
	case AIN_REF_ARRAY_FLOAT:     return AIN_ARRAY_FLOAT;
	case AIN_REF_ARRAY_STRING:    return AIN_ARRAY_STRING;
	case AIN_REF_ARRAY_STRUCT:    return AIN_ARRAY_STRUCT;
	case AIN_REF_FUNC_TYPE:       return AIN_FUNC_TYPE;
	case AIN_REF_ARRAY_FUNC_TYPE: return AIN_ARRAY_FUNC_TYPE;
	case AIN_REF_BOOL:            return AIN_BOOL;
	case AIN_REF_ARRAY_BOOL:      return AIN_ARRAY_BOOL;
	case AIN_REF_LONG_INT:        return AIN_LONG_INT;
	case AIN_REF_ARRAY_LONG_INT:  return AIN_ARRAY_LONG_INT;
	default:                      return type;
	}
}

static bool jaf_type_equal(struct ain_type *a, struct ain_type *b)
{
	enum ain_data_type a_data = strip_ref(a->data), b_data = strip_ref(b->data);
	if (a_data != b_data)
		return false;
	if (a_data == AIN_STRUCT && a->struc != b->struc)
		return false;
	if (a_data == AIN_FUNCTION && a->struc != b->struc)
		return false;
	if (a_data == AIN_FUNC_TYPE && a->struc != b->struc)
		return false;
	return true;
}

static enum ain_data_type jaf_type_check_numeric(struct jaf_expression *expr)
{
	switch (expr->valuetype.data) {
	case AIN_INT:
	case AIN_FLOAT:
	case AIN_BOOL:
	case AIN_LONG_INT:
		return expr->valuetype.data;
	case AIN_REF_INT:
		return AIN_INT;
	case AIN_REF_FLOAT:
		return AIN_FLOAT;
	case AIN_REF_BOOL:
		return AIN_BOOL;
	case AIN_REF_LONG_INT:
		return AIN_LONG_INT;
	default:
		TYPE_ERROR(expr, AIN_INT);
	}
}

static enum ain_data_type jaf_type_check_int(struct jaf_expression *expr)
{
	switch (expr->valuetype.data) {
	case AIN_INT:
	case AIN_LONG_INT:
		return expr->valuetype.data;
	case AIN_REF_INT:
		return AIN_INT;
	case AIN_REF_LONG_INT:
		return AIN_LONG_INT;
	default:
		TYPE_ERROR(expr, AIN_INT);
	}
}

// Determine the result type when combining different numeric types.
// Precedence is: float > lint > int
static enum ain_data_type jaf_merge_types(enum ain_data_type a, enum ain_data_type b)
{
	if (a == b)
		return a;
	if (a == AIN_INT) {
		if (b == AIN_LONG_INT)
			return AIN_LONG_INT;
		if (b == AIN_FLOAT)
			return AIN_FLOAT;
	} else if (a == AIN_LONG_INT) {
		if (b == AIN_INT)
			return AIN_LONG_INT;
		if (b == AIN_FLOAT)
			return AIN_FLOAT;
	} else if (a == AIN_FLOAT) {
		if (a == AIN_INT || a == AIN_LONG_INT)
			return AIN_FLOAT;
	}
	_COMPILER_ERROR(NULL, -1, "Incompatible types");
}

static void jaf_check_types_lvalue(possibly_unused struct jaf_env *env, struct jaf_expression *e)
{
	// TODO: array subscripts
	if (e->type != JAF_EXP_IDENTIFIER && e->type != JAF_EXP_MEMBER && e->type != JAF_EXP_SUBSCRIPT)
		JAF_ERROR(e, "Invalid expression as lvalue");
	switch (e->valuetype.data) {
	case AIN_INT:
	case AIN_FLOAT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_STRING:
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
	case AIN_REF_STRING:
		break;
	default:
		JAF_ERROR(e, "Invalid type as lvalue: %s", ain_strtype(NULL, e->valuetype.data, -1));
	}
}

static void jaf_check_types_unary(struct jaf_env *env, struct jaf_expression *expr)
{
	jaf_derive_types(env, expr->expr);
	switch (expr->op) {
	case JAF_UNARY_PLUS:
	case JAF_UNARY_MINUS:
		expr->valuetype.data = jaf_type_check_numeric(expr->expr);
		break;
	case JAF_PRE_INC:
	case JAF_PRE_DEC:
	case JAF_POST_INC:
	case JAF_POST_DEC:
		jaf_check_types_lvalue(env, expr->expr);
		expr->valuetype.data = jaf_type_check_numeric(expr->expr);
		break;
	case JAF_BIT_NOT:
	case JAF_LOG_NOT:
		expr->valuetype.data = jaf_type_check_int(expr->expr);
		break;
	case JAF_AMPERSAND:
		if (expr->expr->type != JAF_EXP_IDENTIFIER)
			JAF_ERROR(expr, "Non-identifier in '&' expression");
		if (expr->expr->valuetype.data != AIN_FUNCTION)
			JAF_ERROR(expr, "Non-function in '&' expression");
		expr->valuetype.data = AIN_FUNCTION;
		expr->valuetype.struc = expr->expr->valuetype.struc;
		break;
	default:
		COMPILER_ERROR(expr, "Unhandled unary operator");
	}
}

static bool is_string_type(struct jaf_expression *expr)
{
	return expr->valuetype.data == AIN_STRING || expr->valuetype.data == AIN_REF_STRING;
}

static void jaf_check_types_binary(struct jaf_env *env, struct jaf_expression *expr)
{
	jaf_derive_types(env, expr->lhs);
	jaf_derive_types(env, expr->rhs);

	switch (expr->op) {
	// real ops
	case JAF_MULTIPLY:
	case JAF_DIVIDE:
	case JAF_MINUS:
		expr->valuetype.data = jaf_merge_types(jaf_type_check_numeric(expr->lhs), jaf_type_check_numeric(expr->rhs));
		break;
	case JAF_PLUS:
		if (is_string_type(expr->lhs)) {
			if (!is_string_type(expr->rhs))
				TYPE_ERROR(expr->rhs, AIN_STRING);
			expr->valuetype.data = AIN_STRING;
		} else {
			expr->valuetype.data = jaf_merge_types(jaf_type_check_numeric(expr->lhs), jaf_type_check_numeric(expr->rhs));
		}
		break;
	// integer ops
	case JAF_REMAINDER:
		if (is_string_type(expr->lhs)) {
			switch (expr->rhs->valuetype.data) {
			case AIN_INT:
			case AIN_FLOAT:
			case AIN_BOOL:
			case AIN_LONG_INT:
			case AIN_STRING:
			case AIN_REF_INT:
			case AIN_REF_FLOAT:
			case AIN_REF_BOOL:
			case AIN_REF_LONG_INT:
			case AIN_REF_STRING:
				break;
			default:
				TYPE_ERROR(expr->rhs, AIN_STRING); // FIXME: many types ok...
			}
			expr->valuetype.data = AIN_STRING;
			break;
		}
		// fallthrough
	case JAF_LSHIFT:
	case JAF_RSHIFT:
	case JAF_BIT_AND:
	case JAF_BIT_XOR:
	case JAF_BIT_IOR:
		jaf_type_check_int(expr->lhs);
		jaf_type_check_int(expr->rhs);
		expr->valuetype.data = AIN_INT;
		break;
	// comparison ops
	case JAF_LT:
	case JAF_GT:
	case JAF_LTE:
	case JAF_GTE:
	case JAF_EQ:
	case JAF_NEQ:
		if (is_string_type(expr->lhs)) {
			if (!is_string_type(expr->rhs))
				TYPE_ERROR(expr->rhs, AIN_STRING);
			expr->valuetype.data = AIN_INT;
		} else {
			jaf_type_check_numeric(expr->lhs);
			jaf_type_check_numeric(expr->rhs);
			expr->valuetype.data = AIN_INT;
		}
		break;
	// boolean ops
	case JAF_LOG_AND:
	case JAF_LOG_OR:
		jaf_type_check_int(expr->lhs);
		jaf_type_check_int(expr->rhs);
		expr->valuetype.data = AIN_INT;
		break;
	case JAF_ASSIGN:
	case JAF_MUL_ASSIGN:
	case JAF_DIV_ASSIGN:
	case JAF_MOD_ASSIGN:
	case JAF_ADD_ASSIGN:
	case JAF_SUB_ASSIGN:
	case JAF_LSHIFT_ASSIGN:
	case JAF_RSHIFT_ASSIGN:
	case JAF_AND_ASSIGN:
	case JAF_XOR_ASSIGN:
	case JAF_OR_ASSIGN:
		jaf_check_types_lvalue(env, expr->lhs);
		// FIXME: need to coerce types (?)
		jaf_check_type(expr->rhs, &expr->lhs->valuetype);
		expr->valuetype.data = expr->lhs->valuetype.data;
		break;
	case JAF_REF_ASSIGN: // TODO
	default:
		COMPILER_ERROR(expr, "Unhandled binary operator");
	}
}

static void jaf_check_types_ternary(struct jaf_env *env, struct jaf_expression *expr)
{
	jaf_derive_types(env, expr->condition);
	jaf_derive_types(env, expr->consequent);
	jaf_derive_types(env, expr->alternative);

	TYPE_CHECK(expr->condition, AIN_INT);
	if (!jaf_type_equal(&expr->consequent->valuetype, &expr->alternative->valuetype)) {
		// TODO: better error message
		JAF_ERROR(expr, "Mismatched types in conditional expression");
	}

	expr->valuetype = expr->consequent->valuetype;
}

static struct jaf_env_local *jaf_scope_lookup(struct jaf_env *env, const char *name)
{
	for (size_t i = 0; i < env->nr_locals; i++) {
		if (!strcmp(env->locals[i].var->name, name)) {
			return &env->locals[i];
		}
	}
	return NULL;
}

struct ain_variable *jaf_env_lookup(struct jaf_env *env, const char *name, int *var_no)
{
	struct jaf_env *scope = env;
        while (scope) {
		struct jaf_env_local *v = jaf_scope_lookup(scope, name);
		if (v) {
			*var_no = v->no;
			return v->var;
		}
		scope = scope->parent;
	}

	int no = ain_get_global(env->ain, name);
	if (no >= 0) {
		*var_no = no;
		return &env->ain->globals[no];
	}

	return NULL;
}

static void jaf_check_types_identifier(struct jaf_env *env, struct jaf_expression *expr)
{
	int no;
	struct ain_variable *v;
	char *u = conv_output(expr->s->text);
	if (!strcmp(u, "super")) {
		if (!env->fundecl || env->fundecl->super_no <= 0) {
			JAF_ERROR(expr, "'super' used outside of a function override");
		}
		expr->valuetype.data = AIN_FUNCTION;
		expr->valuetype.struc = env->fundecl->super_no;
	} else if ((v = jaf_env_lookup(env, u, &no))) {
		expr->valuetype = v->type;
		expr->ident.var_type = v->var_type;
		expr->ident.var_no = no;
	} else if ((no = ain_get_function(env->ain, u)) >= 0) {
		expr->valuetype.data = AIN_FUNCTION;
		expr->valuetype.struc = no;
	} else {
		JAF_ERROR(expr, "Undefined variable: %s", expr->s->text);
	}
	free(u);
}

static bool ain_wide_type(enum ain_data_type type) {
	switch (type) {
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
		return true;
	default:
		return false;
	}
}

// FIXME: use struct ain_function for function types so that this function isn't needed.
static void jaf_check_types_functype_call(struct jaf_env *env, struct jaf_expression *expr)
{
	unsigned nr_args = expr->call.args ? expr->call.args->nr_items : 0;
	int ft_no = expr->call.fun->valuetype.struc;
	assert(ft_no >= 0 && ft_no <= env->ain->nr_function_types);
	expr->valuetype = env->ain->function_types[ft_no].return_type;
	expr->call.func_no = ft_no;

	struct ain_function_type *f = &env->ain->function_types[ft_no];
	expr->call.args->var_nos = xcalloc(nr_args, sizeof(int));

	// type check arguments against function prototype.
	int arg = 0;
	for (size_t i = 0; i < nr_args; i++, arg++) {
		if (arg >= f->nr_arguments)
			JAF_ERROR(expr, "Too many arguments to function");

		jaf_check_type(expr->call.args->items[i], &f->variables[arg].type);

		expr->call.args->var_nos[i] = arg;
		if (ain_wide_type(f->variables[arg].type.data))
			arg++;
	}
	if (arg != f->nr_arguments)
		JAF_ERROR(expr, "Too few arguments to function");
}

static void jaf_check_types_hll_call(struct jaf_env *env, int lib, struct jaf_expression *expr)
{
	struct jaf_expression *dot = expr->call.fun;
	const char *obj_name = env->ain->libraries[lib].name;
	const char *mbr_name = dot->member.name->text;

	char *fun_name = conv_output(dot->member.name->text);
	int fun = ain_get_library_function(env->ain, lib, fun_name);
	free(fun_name);
	if (fun < 0) {
		JAF_ERROR(expr, "Undefined HLL function: %s.%s", obj_name, mbr_name);
	}
	expr->type = JAF_EXP_HLLCALL;
	expr->call.lib_no = lib;
	expr->call.func_no = fun;
	expr->call.type_param = 0;

	unsigned nr_args = expr->call.args ? expr->call.args->nr_items : 0;
	struct ain_hll_function *def = &env->ain->libraries[lib].functions[fun];
	if (nr_args < (unsigned)def->nr_arguments)
		JAF_ERROR(expr, "Too few arguments to HLL function: %s.%s", obj_name, mbr_name);
	if (nr_args > (unsigned)def->nr_arguments)
		JAF_ERROR(expr, "Too many arguments to HLL function: %s.%s", obj_name, mbr_name);
	for (unsigned i = 0; i < nr_args; i++) {
		jaf_check_type(expr->call.args->items[i], &def->arguments[i].type);
	}
	expr->valuetype = def->return_type;
}

static int get_builtin_lib(struct ain *ain, enum ain_data_type type, struct jaf_expression *expr)
{
	int lib = -1;
	switch (type) {
	case AIN_ARRAY:
		lib = ain_get_library(ain, "Array");
		break;
	case AIN_STRING:
		lib = ain_get_library(ain, "String");
		break;
	default:
		JAF_ERROR(expr, "Methods not supported on built-in type: %s",
			  ain_strtype(ain, type, -1));
	}
	if (lib < 0) {
		JAF_ERROR(expr, "Missing HLL library for built-in type: %s",
			  ain_strtype(ain, type, -1));
	}
	return lib;
}

/*
 * Determines the 3rd argument to CALLHLL for array member functions (ain v11+ only).
 * For ain v11-12 it's just the data type of what's stored in the array.
 * for ain v14 it's... complicated.
 */
static int array_type_param(struct jaf_env *env, struct ain_type *type)
{
	if (AIN_VERSION_GTE(env->ain, 14, 0)) {
		// NOTE: I'm not really sure what the underlying logic is here...
		//       * immediate types get 1
		//       * structs and strings get 2
		//       * 'ref struct' and 'wrap<struct>' get 0x10002
		//       * 'wrap<iwrap<struct>>' gets 0x10003
		switch (type->data) {
		case AIN_REF_STRUCT:
			return 0x10002;
		case AIN_WRAP:
			if (type->array_type->data == AIN_STRUCT)
				return 0x10002;
			if (type->array_type->data == AIN_IFACE_WRAP)
				return 0x10003;
			// XXX: Not totally sure if this is right... maybe always 2 here?
			return array_type_param(env, type->array_type);
		case AIN_ARRAY:
			return array_type_param(env, type->array_type);
		case AIN_INT:
		case AIN_FLOAT:
			return 1;
		case AIN_STRING:
		case AIN_STRUCT:
			return 2;
		default:
			// XXX: assume 2 for unconfirmed types...
			WARNING("Assuming HLL type param is 2 for type: %s", ain_strtype(env->ain, type->data, -1));
			return 2;
		}
	}
	return type->array_type->data;
}

/*
 * Type check a method call on a built-in type (ain v11+ only).
 * This is broadly similar to checking a regular HLL call.
 */
static void jaf_check_types_builtin_hll_call(struct jaf_env *env, struct ain_type *type,
					     struct jaf_expression *expr)
{
	struct jaf_expression *dot = expr->call.fun;
	int lib = get_builtin_lib(env->ain, type->data, expr);
	const char *obj_name = env->ain->libraries[lib].name;
	const char *mbr_name = dot->member.name->text;

	char *fun_name = conv_output(dot->member.name->text);
	int fun = ain_get_library_function(env->ain, lib, fun_name);
	free(fun_name);
	if (fun < 0) {
		JAF_ERROR(expr, "Undefined built-in method: %s.%s", obj_name, mbr_name);
	}
	expr->type = JAF_EXP_BUILTIN_CALL;
	expr->call.lib_no = lib;
	expr->call.func_no = fun;
	if (type->data == AIN_ARRAY) {
		expr->call.type_param = array_type_param(env, type->array_type);
	} else {
		expr->call.type_param = 0;
	}

	unsigned nr_args = expr->call.args ? expr->call.args->nr_items : 0;
	struct ain_hll_function *def = &env->ain->libraries[lib].functions[fun];
	if (nr_args+1 < (unsigned)def->nr_arguments)
		JAF_ERROR(expr, "Too few arguments to built-in method: %s.%s", obj_name, mbr_name);
	if (nr_args+1 > (unsigned)def->nr_arguments)
		JAF_ERROR(expr, "Too many arguments to built-in method: %s.%s", obj_name, mbr_name);
	for (unsigned i = 0; i < nr_args; i++) {
		jaf_check_type(expr->call.args->items[i], &def->arguments[i+1].type);
	}
	expr->valuetype = def->return_type;
}

static void jaf_check_types_sys_call(struct jaf_expression *expr)
{
	struct jaf_expression *dot = expr->call.fun;
	const char *mbr_name = dot->member.name->text;

	expr->type = JAF_EXP_SYSCALL;
	expr->call.func_no = -1;
	for (int i = 0; i < NR_SYSCALLS; i++) {
		if (!strcmp(mbr_name, syscalls[i].name+7)) {
			expr->call.func_no = i;
			break;
		}
	}
	if (expr->call.func_no == -1) {
		JAF_ERROR(expr, "Invalid system call: system.%s", mbr_name);
	}

	unsigned nr_args = expr->call.args ? expr->call.args->nr_items : 0;
	for (unsigned i = 0; i < nr_args; i++) {
		struct ain_type type = {
			.data = syscalls[expr->call.func_no].argtypes[i],
			.struc = -1,
			.rank = 0
		};
		jaf_check_type(expr->call.args->items[i], &type);
	}
	expr->valuetype = syscalls[expr->call.func_no].return_type;
}

static bool jaf_check_types_special_call(struct jaf_env *env, struct jaf_expression *expr)
{
	if (expr->call.fun->type != JAF_EXP_MEMBER)
		return false;

	struct jaf_expression *obj = expr->call.fun->member.struc;
	if (obj->type == JAF_EXP_IDENTIFIER) {
		char *obj_name = conv_output(obj->s->text);
		int lib = ain_get_library(env->ain, obj_name);

		// HLL call
		if (lib >= 0) {
			jaf_check_types_hll_call(env, lib, expr);
			free(obj_name);
			return true;
		}
		// system call
		if (!strcmp(obj_name, "system")) {
			jaf_check_types_sys_call(expr);
			free(obj_name);
			return true;
		}
		free(obj_name);
	}

	jaf_derive_types(env, obj);
	if (AIN_VERSION_GTE(env->ain, 11, 0)) {
		switch (obj->valuetype.data) {
		case AIN_INT:
		case AIN_FLOAT:
		case AIN_STRING:
		case AIN_ARRAY:
		case AIN_REF_INT:
		case AIN_REF_FLOAT:
		case AIN_REF_STRING:
		case AIN_REF_ARRAY:
			jaf_check_types_builtin_hll_call(env, &obj->valuetype, expr);
			return true;
			break;
		default:
			break;
		}
	} else {
		switch (obj->valuetype.data) {
		case AIN_INT:
		case AIN_FLOAT:
		case AIN_STRING:
		case AIN_ARRAY_TYPE:
		case AIN_REF_INT:
		case AIN_REF_FLOAT:
		case AIN_REF_STRING:
		case AIN_REF_ARRAY_TYPE:
			JAF_ERROR(expr, "Methods not supported on built-in type: %s",
				  ain_strtype(env->ain, obj->valuetype.data, -1));
			break;
		default:
			break;
		}
	}
	return false;
}

static void jaf_check_types_funcall(struct jaf_env *env, struct jaf_expression *expr)
{
	unsigned nr_args = expr->call.args ? expr->call.args->nr_items : 0;
	for (size_t i = 0; i < nr_args; i++) {
		jaf_derive_types(env, expr->call.args->items[i]);
	}

	// handle HLL, system calls
	if (jaf_check_types_special_call(env, expr))
		return;

	jaf_derive_types(env, expr->call.fun);
	if (expr->call.fun->valuetype.data != AIN_FUNCTION && expr->call.fun->valuetype.data != AIN_FUNC_TYPE)
		TYPE_ERROR(expr->call.fun, AIN_FUNC_TYPE);

	if (expr->call.fun->valuetype.data == AIN_FUNC_TYPE) {
		jaf_check_types_functype_call(env, expr);
		return;
	}

	int func_no = expr->call.fun->valuetype.struc;
	assert(func_no >= 0 && func_no < env->ain->nr_functions);
	expr->valuetype = env->ain->functions[func_no].return_type;
	expr->call.func_no = func_no;

	struct ain_function *f = &env->ain->functions[func_no];
	expr->call.args->var_nos = xcalloc(nr_args, sizeof(int));

	// type check arguments against function prototype.
	int arg = 0;
	for (size_t i = 0; i < nr_args; i++, arg++) {
		if (arg >= f->nr_args)
			JAF_ERROR(expr, "Too many arguments to function");

		jaf_check_type(expr->call.args->items[i], &f->vars[arg].type);

		expr->call.args->var_nos[i] = arg;
		if (ain_wide_type(f->vars[arg].type.data))
			arg++;
	}
	if (arg != f->nr_args)
		JAF_ERROR(expr, "Too few arguments to function");
}

static bool is_struct_type(struct jaf_expression *e)
{
	return e->valuetype.data == AIN_STRUCT || e->valuetype.data == AIN_REF_STRUCT;
}

static void jaf_check_types_member(struct jaf_env *env, struct jaf_expression *expr)
{
	jaf_derive_types(env, expr->member.struc);
	if (!is_struct_type(expr->member.struc))
		TYPE_ERROR(expr->member.struc, AIN_STRUCT);

	expr->member.member_no = -1;
	char *u = conv_output(expr->member.name->text);
	struct ain_struct *s = &env->ain->structures[expr->member.struc->valuetype.struc];
	for (int i = 0; i < s->nr_members; i++) {
		if (!strcmp(s->members[i].name, u)) {
			expr->valuetype = s->members[i].type;
			expr->member.member_no = i;
			break;
		}
	}
	if (expr->member.member_no == -1)
		JAF_ERROR(expr, "Invalid struct member name: %s", u);
	free(u);
}

static void jaf_check_types_seq(struct jaf_env *env, struct jaf_expression *expr)
{
	jaf_derive_types(env, expr->seq.head);
	jaf_derive_types(env, expr->seq.tail);
	expr->valuetype = expr->seq.tail->valuetype;
}

static enum ain_data_type array_data_type(struct ain_type *type)
{
	switch (type->data) {
	case AIN_ARRAY_INT:       case AIN_REF_ARRAY_INT:       return AIN_INT;
	case AIN_ARRAY_FLOAT:     case AIN_REF_ARRAY_FLOAT:     return AIN_FLOAT;
	case AIN_ARRAY_STRING:    case AIN_REF_ARRAY_STRING:    return AIN_STRING;
	case AIN_ARRAY_STRUCT:    case AIN_REF_ARRAY_STRUCT:    return AIN_STRUCT;
	case AIN_ARRAY_FUNC_TYPE: case AIN_REF_ARRAY_FUNC_TYPE: return AIN_FUNC_TYPE;
	case AIN_ARRAY_BOOL:      case AIN_REF_ARRAY_BOOL:      return AIN_BOOL;
	case AIN_ARRAY_LONG_INT:  case AIN_REF_ARRAY_LONG_INT:  return AIN_LONG_INT;
	case AIN_ARRAY_DELEGATE:  case AIN_REF_ARRAY_DELEGATE:  return AIN_DELEGATE;
	case AIN_ARRAY: return type->array_type->data;
	default:
		return AIN_VOID;
	}
}

static void jaf_type_check_array(struct jaf_expression *expr)
{
	switch (expr->valuetype.data) {
	case AIN_ARRAY:
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE:
		return;
	default:
		TYPE_ERROR(expr, AIN_ARRAY);
	}
}

static void array_deref_type(struct ain_type *dst, struct ain_type *src)
{
	if (src->rank > 1) {
		dst->data = src->data;
		dst->struc = src->struc;
		dst->rank = src->rank - 1;
	} else {
		assert(src->rank == 1);
		dst->data = array_data_type(src);
		dst->struc = src->struc;
		dst->rank = 0;
	}
}

static void jaf_check_types_subscript(struct jaf_env *env, struct jaf_expression *expr)
{
	jaf_derive_types(env, expr->subscript.expr);
	jaf_derive_types(env, expr->subscript.index);
	jaf_type_check_array(expr->subscript.expr);
	jaf_type_check_int(expr->subscript.index);

	array_deref_type(&expr->valuetype, &expr->subscript.expr->valuetype);
}

void jaf_derive_types(struct jaf_env *env, struct jaf_expression *expr)
{
	switch (expr->type) {
	case JAF_EXP_VOID:
		expr->valuetype.data = AIN_VOID;
		break;
	case JAF_EXP_INT:
	case JAF_EXP_CHAR:
		expr->valuetype.data = AIN_INT;
		break;
	case JAF_EXP_FLOAT:
		expr->valuetype.data = AIN_FLOAT;
		break;
	case JAF_EXP_STRING:
		expr->valuetype.data = AIN_STRING;
		break;
	case JAF_EXP_IDENTIFIER:
		jaf_check_types_identifier(env, expr);
		break;
	case JAF_EXP_UNARY:
		jaf_check_types_unary(env, expr);
		break;
	case JAF_EXP_BINARY:
		jaf_check_types_binary(env, expr);
		break;
	case JAF_EXP_TERNARY:
		jaf_check_types_ternary(env, expr);
		break;
	case JAF_EXP_FUNCALL:
		jaf_check_types_funcall(env, expr);
		break;
	case JAF_EXP_CAST:
		expr->valuetype.data = jaf_to_ain_simple_type(expr->cast.type);
		break;
	case JAF_EXP_MEMBER:
		jaf_check_types_member(env, expr);
		break;
	case JAF_EXP_SEQ:
		jaf_check_types_seq(env, expr);
		break;
	case JAF_EXP_SUBSCRIPT:
		jaf_check_types_subscript(env, expr);
		break;
	case JAF_EXP_SYSCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_BUILTIN_CALL:
		// these should be JAF_EXP_FUNCALLs initially
		JAF_ERROR(expr, "Unexpected expression type");
	}
}

static bool is_numeric(enum ain_data_type type)
{
	switch (type) {
	case AIN_INT:
	case AIN_FLOAT:
	case AIN_LONG_INT:
	case AIN_BOOL:
		return true;
	default:
		return false;
	}
}

void jaf_check_type(struct jaf_expression *expr, struct ain_type *type)
{
	if (!jaf_type_equal(&expr->valuetype, type)) {
		// numeric types are compatible (implicit cast)
		if (is_numeric(expr->valuetype.data) && is_numeric(type->data))
			return;
		TYPE_ERROR(expr, type->data);
	}
}
