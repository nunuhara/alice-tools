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
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/jaf.h"

// TODO: better error messages
#define TYPE_ERROR(expr, expected) JAF_ERROR(expr, "Type error (expected %s; got %s)", strdup(ain_strtype(NULL, expected, -1)), strdup(ain_strtype(NULL, (expr)->valuetype.data, -1)))
#define TYPE_CHECK(expr, expected) { if ((expr)->valuetype.data != expected) TYPE_ERROR(expr, expected); }

static enum ain_data_type strip_ref(struct ain_type *type)
{
	switch (type->data) {
	case AIN_REF_INT:             return AIN_INT;
	case AIN_REF_FLOAT:           return AIN_FLOAT;
	case AIN_REF_STRING:          return AIN_STRING;
	case AIN_REF_STRUCT:          return AIN_STRUCT;
	case AIN_REF_ENUM:            return AIN_ENUM;
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
	case AIN_REF_ARRAY:           return AIN_ARRAY;
	case AIN_WRAP:
		assert(type->array_type);
		return type->array_type->data;
	default:                      return type->data;
	}
}

static enum ain_data_type add_ref(struct ain_type *type)
{
	switch (type->data) {
	case AIN_INT:             return AIN_REF_INT;
	case AIN_FLOAT:           return AIN_REF_FLOAT;
	case AIN_STRING:          return AIN_REF_STRING;
	case AIN_STRUCT:          return AIN_REF_STRUCT;
	case AIN_ENUM:            return AIN_REF_ENUM;
	case AIN_ARRAY_INT:       return AIN_REF_ARRAY_INT;
	case AIN_ARRAY_FLOAT:     return AIN_REF_ARRAY_FLOAT;
	case AIN_ARRAY_STRING:    return AIN_REF_ARRAY_STRING;
	case AIN_ARRAY_STRUCT:    return AIN_REF_ARRAY_STRUCT;
	case AIN_FUNC_TYPE:       return AIN_REF_FUNC_TYPE;
	case AIN_ARRAY_FUNC_TYPE: return AIN_REF_ARRAY_FUNC_TYPE;
	case AIN_BOOL:            return AIN_REF_BOOL;
	case AIN_ARRAY_BOOL:      return AIN_REF_ARRAY_BOOL;
	case AIN_LONG_INT:        return AIN_REF_LONG_INT;
	case AIN_ARRAY_LONG_INT:  return AIN_REF_ARRAY_LONG_INT;
	case AIN_ARRAY:           return AIN_REF_ARRAY;
	default:                  return type->data;
	}

}

static bool jaf_type_equal(struct ain_type *a, struct ain_type *b)
{
	enum ain_data_type a_data = strip_ref(a), b_data = strip_ref(b);
	// HACK: treat lint as int
	if (a_data == AIN_LONG_INT)
		a_data = AIN_INT;
	if (b_data == AIN_LONG_INT)
		b_data = AIN_INT;
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
	case AIN_ENUM:
		return expr->valuetype.data;
	case AIN_REF_INT:
		return AIN_INT;
	case AIN_REF_FLOAT:
		return AIN_FLOAT;
	case AIN_REF_BOOL:
		return AIN_BOOL;
	case AIN_REF_LONG_INT:
		return AIN_LONG_INT;
	case AIN_REF_ENUM:
		return AIN_ENUM;
	default:
		TYPE_ERROR(expr, AIN_INT);
	}
}

static enum ain_data_type jaf_type_check_int(struct jaf_expression *expr)
{
	switch (expr->valuetype.data) {
	case AIN_INT:
	case AIN_LONG_INT:
	case AIN_BOOL:
	case AIN_ENUM:
		return expr->valuetype.data;
	case AIN_REF_INT:
		return AIN_INT;
	case AIN_REF_LONG_INT:
		return AIN_LONG_INT;
	case AIN_REF_BOOL:
		return AIN_BOOL;
	case AIN_REF_ENUM:
		return AIN_ENUM;
	default:
		TYPE_ERROR(expr, AIN_INT);
	}
}

void jaf_check_type_lvalue(possibly_unused struct jaf_env *env, struct jaf_expression *e)
{
	switch (e->type) {
	case JAF_EXP_IDENTIFIER:
	case JAF_EXP_MEMBER:
	case JAF_EXP_SUBSCRIPT:
	case JAF_EXP_NEW:
		break;
	default:
		JAF_ERROR(e, "Invalid expression as lvalue");
	}

	switch (e->valuetype.data) {
	case AIN_INT:
	case AIN_FLOAT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_STRING:
	case AIN_ENUM:
	case AIN_REF_INT:
	case AIN_REF_FLOAT:
	case AIN_REF_BOOL:
	case AIN_REF_LONG_INT:
	case AIN_REF_STRING:
	case AIN_REF_STRUCT:
	case AIN_REF_ENUM:
		break;
	default:
		JAF_ERROR(e, "Invalid type as lvalue: %s", ain_strtype(NULL, e->valuetype.data, -1));
	}
}

static void jaf_check_types_unary(struct jaf_env *env, struct jaf_expression *expr)
{
	switch (expr->op) {
	case JAF_UNARY_PLUS:
	case JAF_UNARY_MINUS:
		expr->valuetype.data = jaf_type_check_numeric(expr->expr);
		break;
	case JAF_PRE_INC:
	case JAF_PRE_DEC:
	case JAF_POST_INC:
	case JAF_POST_DEC:
		jaf_check_type_lvalue(env, expr->expr);
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

#define CAST(v, t) ( \
	*v = jaf_cast_expression(JAF_##t, *v), \
	(*v)->valuetype.data = AIN_##t \
)

static enum ain_data_type jaf_type_check_weak_arithmetic(struct jaf_expression **a,
		struct jaf_expression **b)
{
	enum ain_data_type a_type = jaf_type_check_numeric(*a);
	enum ain_data_type b_type = jaf_type_check_numeric(*b);
	if (a_type == b_type)
		return a_type;
	if (a_type == AIN_FLOAT)
		return CAST(b, FLOAT);
	if (b_type == AIN_FLOAT)
		return CAST(a, FLOAT);
	if (a_type == AIN_LONG_INT)
		return CAST(b, LONG_INT);
	if (b_type == AIN_LONG_INT)
		return CAST(a, LONG_INT);
	return a_type;
}

static void jaf_type_check_weak_assign(enum jaf_operator op, struct jaf_expression *lvalue, struct jaf_expression **rvalue)
{
	enum ain_data_type lv_type = strip_ref(&lvalue->valuetype);
	if (lv_type == AIN_INT) {
		if (jaf_type_check_numeric(*rvalue) != AIN_INT)
			CAST(rvalue, INT);
	} else if (lv_type == AIN_FLOAT) {
		switch (op) {
		case JAF_LSHIFT_ASSIGN:
		case JAF_RSHIFT_ASSIGN:
		case JAF_AND_ASSIGN:
		case JAF_XOR_ASSIGN:
		case JAF_OR_ASSIGN:
			JAF_ERROR(lvalue, "Invalid lvalue for assign operator");
			break;
		default:
			break;
		}
		if (jaf_type_check_numeric(*rvalue) != AIN_FLOAT)
			CAST(rvalue, FLOAT);
	} else if (lv_type == AIN_LONG_INT) {
		if (jaf_type_check_numeric(*rvalue) != AIN_LONG_INT)
			CAST(rvalue, LONG_INT);
	} else if (lv_type == AIN_BOOL) {
		if (jaf_type_check_numeric(*rvalue) != AIN_BOOL)
			CAST(rvalue, BOOL);
	} else if (lv_type == AIN_STRING) {
		if (op != JAF_ASSIGN && op != JAF_ADD_ASSIGN)
			TYPE_ERROR(lvalue, AIN_INT); // FIXME: many types ok...
		if (!is_string_type(*rvalue))
			TYPE_ERROR(*rvalue, AIN_STRING);
	}
}

#undef CAST

static void jaf_check_types_binary(struct jaf_env *env, struct jaf_expression *expr)
{
	switch (expr->op) {
	// real ops
	case JAF_MULTIPLY:
	case JAF_DIVIDE:
	case JAF_MINUS:
		expr->valuetype.data = jaf_type_check_weak_arithmetic(&expr->lhs, &expr->rhs);
		break;
	case JAF_PLUS:
		if (is_string_type(expr->lhs)) {
			if (!is_string_type(expr->rhs))
				TYPE_ERROR(expr->rhs, AIN_STRING);
			expr->valuetype.data = AIN_STRING;
		} else {
			expr->valuetype.data = jaf_type_check_weak_arithmetic(&expr->lhs, &expr->rhs);
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
			case AIN_ENUM:
			case AIN_REF_INT:
			case AIN_REF_FLOAT:
			case AIN_REF_BOOL:
			case AIN_REF_LONG_INT:
			case AIN_REF_STRING:
			case AIN_REF_ENUM:
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
			jaf_type_check_weak_arithmetic(&expr->lhs, &expr->rhs);
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
		jaf_check_type_lvalue(env, expr->lhs);
		jaf_type_check_weak_assign(expr->op, expr->lhs, &expr->rhs);
		expr->valuetype.data = expr->lhs->valuetype.data;
		break;
	default:
		COMPILER_ERROR(expr, "Unhandled binary operator");
	}
}

static void jaf_check_types_ternary(possibly_unused struct jaf_env *env, struct jaf_expression *expr)
{
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

static struct ain_variable *jaf_global_lookup(struct jaf_env *env, const char *name, int *var_no)
{
	int no = ain_get_global(env->ain, name);
	if (no < 0)
		return NULL;
	*var_no = no;
	return &env->ain->globals[no];
}

static void jaf_check_types_identifier(struct jaf_env *env, struct jaf_expression *expr)
{
	int no;
	struct ain_variable *v;
	struct jaf_env_local *local;
	char *u = conv_output(expr->ident.name->text);
	if (!strcmp(u, "super")) {
		if (!env->fundecl || env->fundecl->super_no <= 0) {
			JAF_ERROR(expr, "'super' used outside of a function override");
		}
		expr->valuetype.data = AIN_SUPER;
		expr->valuetype.struc = env->fundecl->super_no;
	} else if (AIN_VERSION_LT(env->ain, 11, 0) && !strcmp(u, "system")) {
		expr->valuetype.data = AIN_SYSTEM;
	} else if ((local = jaf_env_lookup(env, u))) {
		if (local->is_const) {
			expr->valuetype.data = local->val.data_type;
			expr->valuetype.struc = 0;
			expr->valuetype.rank = 0;
			expr->ident.is_const = true;
			expr->ident.val = local->val;
		} else {
			expr->valuetype = local->var->type;
			expr->ident.var_type = local->var->var_type;
			expr->ident.var_no = local->no;
		}
	} else if ((v = jaf_global_lookup(env, u, &no))) {
		expr->valuetype = v->type;
		expr->ident.var_type = v->var_type;
		expr->ident.var_no = no;
	} else if ((no = ain_get_function(env->ain, u)) >= 0) {
		expr->valuetype.data = AIN_FUNCTION;
		expr->valuetype.struc = no;
	} else if ((no = ain_get_library(env->ain, u)) >= 0) {
		expr->valuetype.data = AIN_LIBRARY;
		expr->valuetype.struc = no;
	} else {
		JAF_ERROR(expr, "Undefined variable: %s", expr->ident.name->text);
	}
	free(u);
}

static void jaf_check_types_this(struct jaf_env *env, struct jaf_expression *expr)
{
	if (env->func_no < 0) {
		JAF_ERROR(expr, "'this' outside of method body");
	}
	if (env->ain->functions[env->func_no].struct_type < 0) {
		JAF_ERROR(expr, "'this' outside of method body");
	}
	expr->valuetype.data = AIN_STRUCT;
	expr->valuetype.struc = env->ain->functions[env->func_no].struct_type;
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

static int get_builtin_lib(struct ain *ain, enum ain_data_type type, struct jaf_expression *expr)
{
	int lib = -1;
	switch (type) {
	case AIN_ARRAY:
	case AIN_REF_ARRAY:
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
		JAF_ERROR(expr, "Methods not supported on built-in type: %s (missing HLL library)",
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
		case AIN_REF_ARRAY:
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
	return type->data;
}

static void jaf_check_function_arguments(struct jaf_expression *expr, struct jaf_argument_list *args, struct ain_function *f)
{
	int arg = 0;

	args->var_nos = xcalloc(args->nr_items, sizeof(int));
	for (unsigned i = 0; i < args->nr_items; i++, arg++) {
		if (arg >= f->nr_args)
			JAF_ERROR(expr, "Too many arguments to function %s", conv_utf8(f->name));

		jaf_check_type(args->items[i], &f->vars[arg].type);

		args->var_nos[i] = arg;
		if (ain_wide_type(f->vars[arg].type.data))
			arg++;
	}
	if (arg != f->nr_args)
		JAF_ERROR(expr, "Too few arguments to function %s", conv_utf8(f->name));
}

static void jaf_check_functype_arguments(struct jaf_expression *expr, struct jaf_argument_list *args, struct ain_function_type *f)
{
	int arg = 0;

	args->var_nos = xcalloc(args->nr_items, sizeof(int));
	for (unsigned i = 0; i < args->nr_items; i++, arg++) {
		if (arg >= f->nr_arguments)
			JAF_ERROR(expr, "Too many arguments to function type %s", conv_utf8(f->name));

		jaf_check_type(args->items[i], &f->variables[arg].type);

		args->var_nos[i] = arg;
		if (ain_wide_type(f->variables[arg].type.data))
			arg++;
	}
	if (arg != f->nr_arguments)
		JAF_ERROR(expr, "Too few arguments to function type %s", conv_utf8(f->name));
}

static struct string *make_method_name(struct ain *ain, int struct_type, const char *_name)
{
	char *name = conv_output(_name);
	const char *struct_name = ain->structures[struct_type].name;
	struct string *method_name = make_string(struct_name, strlen(struct_name));
	string_push_back(&method_name, '@');
	string_append_cstr(&method_name, name, strlen(name));
	free(name);
	return method_name;
}

static void jaf_check_types_function_call(struct jaf_env *env, struct jaf_expression *expr)
{
	int func_no = expr->call.fun->valuetype.struc;
	assert(func_no >= 0 && func_no < env->ain->nr_functions);

	jaf_check_function_arguments(expr, expr->call.args, &env->ain->functions[func_no]);
	expr->valuetype = env->ain->functions[func_no].return_type;
	expr->call.func_no = func_no;
}

static void jaf_check_types_functype_call(struct jaf_env *env, struct jaf_expression *expr)
{
	int ft_no = expr->call.fun->valuetype.struc;
	assert(ft_no >= 0 && ft_no < env->ain->nr_function_types);

	jaf_check_functype_arguments(expr, expr->call.args, &env->ain->function_types[ft_no]);
	expr->valuetype = env->ain->function_types[ft_no].return_type;
	expr->call.func_no = ft_no;
}

static void jaf_check_types_system_call(possibly_unused struct jaf_env *env, struct jaf_expression *expr)
{
	expr->type = JAF_EXP_SYSCALL;
	expr->call.func_no = expr->call.fun->member.member_no;

	for (size_t i = 0; i < expr->call.args->nr_items; i++) {
		struct ain_type type = {
			.data = syscalls[expr->call.func_no].argtypes[i],
			.struc = -1,
			.rank = 0
		};
		jaf_check_type(expr->call.args->items[i], &type);
	}
	expr->valuetype = syscalls[expr->call.func_no].return_type;
}

static void jaf_check_hll_argument(struct jaf_env *env, struct jaf_expression *arg, struct ain_type *type, struct ain_type *type_param)
{
	if (type && (type->data == AIN_HLL_PARAM || type->data == AIN_REF_HLL_PARAM)) {
		if (type_param->data != AIN_ARRAY && type_param->data != AIN_REF_ARRAY)
			COMPILER_ERROR(arg, "Expected array as type param");
		jaf_check_type(arg, type_param->array_type);
	} else if (type->data == AIN_IMAIN_SYSTEM) {
		jaf_type_check_int(arg);
	} else if (!AIN_VERSION_GTE(env->ain, 14, 0) && (type->data == AIN_STRUCT || type->data == AIN_REF_STRUCT)) {
		// XXX: special case since hll types are data-only (until v14+)
		if (arg->valuetype.data != AIN_STRUCT) {
			TYPE_ERROR(arg, type->data);
		}
	} else {
		jaf_check_type(arg, type);
	}
}

static void jaf_check_types_hll_call(struct jaf_env *env, struct jaf_expression *expr, struct ain_type *type)
{
	expr->type = JAF_EXP_HLLCALL;
	expr->call.lib_no = expr->call.fun->member.object_no;
	expr->call.func_no = expr->call.fun->member.member_no;
	if (type && (type->data == AIN_ARRAY || type->data == AIN_REF_ARRAY)) {
		assert(type->array_type);
		expr->call.type_param = array_type_param(env, type->array_type);
	} else {
		expr->call.type_param = 0;
	}

	const char *obj_name = env->ain->libraries[expr->call.lib_no].name;
	const char *mbr_name = expr->call.fun->member.name->text;

	unsigned nr_args = expr->call.args ? expr->call.args->nr_items : 0;
	struct ain_hll_function *def = &env->ain->libraries[expr->call.lib_no].functions[expr->call.func_no];
	if (nr_args < (unsigned)def->nr_arguments)
		JAF_ERROR(expr, "Too few arguments to HLL function: %s.%s", obj_name, mbr_name);
	if (nr_args > (unsigned)def->nr_arguments)
		JAF_ERROR(expr, "Too many arguments to HLL function: %s.%s", obj_name, mbr_name);
	// FIXME: multi-valued arguments?
	for (unsigned i = 0; i < nr_args; i++) {
		jaf_check_hll_argument(env, expr->call.args->items[i], &def->arguments[i].type, type);
		//jaf_check_type(expr->call.args->items[i], &def->arguments[i].type);
	}

	if (def->return_type.data == AIN_HLL_PARAM) {
		expr->valuetype = *type->array_type;
	} else if (def->return_type.data == AIN_REF_HLL_PARAM) {
		expr->valuetype = *type->array_type;
		expr->valuetype.data = add_ref(&expr->valuetype);
	} else {
		expr->valuetype = def->return_type;
	}
}

static void jaf_check_types_method_call(struct jaf_env *env, struct jaf_expression *expr)
{
	struct jaf_expression *obj = expr->call.fun->member.struc;
	assert(obj->valuetype.data == AIN_STRUCT || obj->valuetype.data == AIN_REF_STRUCT);
	assert(obj->valuetype.struc >= 0);

	int method_no = expr->call.fun->member.member_no;
	expr->type = JAF_EXP_METHOD_CALL;
	expr->call.func_no = method_no;
	jaf_check_function_arguments(expr, expr->call.args, &env->ain->functions[method_no]);
	expr->valuetype = env->ain->functions[method_no].return_type;
}

static void jaf_check_types_super_call(struct jaf_env *env, struct jaf_expression *expr)
{
	int method_no = expr->call.fun->valuetype.struc;
	expr->type = JAF_EXP_SUPER_CALL;
	expr->call.func_no = method_no;
	jaf_check_function_arguments(expr, expr->call.args, &env->ain->functions[method_no]);
	expr->valuetype = env->ain->functions[method_no].return_type;
}

static const char *_builtin_type_name(enum ain_data_type type)
{
	switch (type) {
	case AIN_INT: return "Int";
	case AIN_FLOAT: return "Float";
	case AIN_STRING: return "String";
	case AIN_ARRAY: return "Array";
	default: _COMPILER_ERROR(NULL, -1, "Invalid type for builtin");
	}
}

static struct string *builtin_type_name(enum ain_data_type type)
{
	const char *name = _builtin_type_name(type);
	return make_string(name, strlen(name));
}

/*
 * NOTE: We rewrite the expression here into a regular HLL call to avoid duplicated code.
 */
static void jaf_check_types_builtin_hll_call(struct jaf_env *env, struct jaf_expression *expr)
{
	struct jaf_expression *obj = expr->call.fun->member.struc;
	expr->call.fun->member.struc = jaf_identifier(builtin_type_name(obj->valuetype.data)); // dummy expression

	// put obj at head of argument list
	struct jaf_argument_list *args = expr->call.args;
	args->items = xrealloc_array(args->items, args->nr_items, args->nr_items+1, sizeof(struct jaf_expression*));
	for (unsigned i = args->nr_items; i > 0; i--) {
		args->items[i] = args->items[i-1];
	}
	args->items[0] = obj;
	args->nr_items++;

	jaf_check_types_hll_call(env, expr, &obj->valuetype);
}

struct builtin {
	enum ain_data_type return_type;
	enum ain_data_type type;
	const char *name;
	int nr_args;
	enum opcode opcode;
};

static struct builtin builtins[] = {
	[JAF_INT_STRING]        = { AIN_STRING, AIN_INT, "String",        0, I_STRING },
	[JAF_FLOAT_STRING]      = { AIN_STRING, AIN_FLOAT, "String",      0, FTOS },
	[JAF_STRING_INT]        = { AIN_INT,    AIN_STRING, "Int",        0, STOI },
	[JAF_STRING_LENGTH]     = { AIN_INT,    AIN_STRING, "Length",     0, S_LENGTH },
	[JAF_STRING_LENGTHBYTE] = { AIN_INT,    AIN_STRING, "LengthByte", 0, S_LENGTHBYTE },
	[JAF_STRING_EMPTY]      = { AIN_INT,    AIN_STRING, "Empty",      0, S_EMPTY },
	[JAF_STRING_FIND]       = { AIN_INT,    AIN_STRING, "Find",       1, S_FIND },
	[JAF_STRING_GETPART]    = { AIN_STRING, AIN_STRING, "GetPart",    2, S_GETPART },
	[JAF_STRING_PUSHBACK]   = { AIN_VOID,   AIN_STRING, "PushBack",   1, S_PUSHBACK },
	[JAF_STRING_POPBACK]    = { AIN_VOID,   AIN_STRING, "PopBack",    0, S_POPBACK },
	[JAF_STRING_ERASE]      = { AIN_VOID,   AIN_STRING, "Erase",      1, S_ERASE },
	[JAF_ARRAY_ALLOC]       = { AIN_VOID,   AIN_ARRAY, "Alloc",       1, A_ALLOC },
	[JAF_ARRAY_REALLOC]     = { AIN_VOID,   AIN_ARRAY, "Realloc",     1, A_REALLOC },
	[JAF_ARRAY_FREE]        = { AIN_VOID,   AIN_ARRAY, "Free",        0, A_FREE },
	[JAF_ARRAY_NUMOF]       = { AIN_INT,    AIN_ARRAY, "Numof",       0, A_NUMOF },
	[JAF_ARRAY_COPY]        = { AIN_INT,    AIN_ARRAY, "Copy",        4, A_COPY },
	[JAF_ARRAY_FILL]        = { AIN_INT,    AIN_ARRAY, "Fill",        3, A_FILL },
	[JAF_ARRAY_PUSHBACK]    = { AIN_VOID,   AIN_ARRAY, "PushBack",    1, A_PUSHBACK },
	[JAF_ARRAY_POPBACK]     = { AIN_VOID,   AIN_ARRAY, "PopBack",     0, A_POPBACK },
	[JAF_ARRAY_EMPTY]       = { AIN_INT,    AIN_ARRAY, "Empty",       0, A_EMPTY },
	[JAF_ARRAY_ERASE]       = { AIN_INT,    AIN_ARRAY, "Erase",       1, A_ERASE },
	[JAF_ARRAY_INSERT]      = { AIN_VOID,   AIN_ARRAY, "Insert",      2, A_INSERT },
	[JAF_ARRAY_SORT]        = { AIN_VOID,   AIN_ARRAY, "Sort",        1, A_SORT },
};

static void jaf_check_types_builtin_call(struct jaf_env *env, struct jaf_expression *expr)
{
	struct builtin *builtin = &builtins[expr->call.fun->member.member_no];
	if (expr->call.args->nr_items != builtin->nr_args) {
		JAF_ERROR(expr, "Too many arguments to builtin method: %s.%s",
				_builtin_type_name(builtin->type), builtin->name);
	}

	expr->type = JAF_EXP_BUILTIN_CALL;
	expr->valuetype.data = builtin->return_type;
	expr->call.func_no = builtin->opcode;

	struct ain_type int_type = { .data = AIN_INT };
	struct ain_type str_type = { .data = AIN_STRING };
	struct jaf_argument_list *args = expr->call.args;
	switch ((enum jaf_builtin_method)expr->call.fun->member.member_no) {
	case JAF_STRING_FIND:
		jaf_check_type(args->items[0], &str_type);
		break;
	case JAF_STRING_GETPART:
		jaf_check_type(args->items[0], &int_type);
		jaf_check_type(args->items[1], &int_type);
		break;
	case JAF_STRING_PUSHBACK:
		jaf_check_type(args->items[0], &int_type);
		break;
	case JAF_STRING_ERASE:
		jaf_check_type(args->items[0], &int_type);
		break;
	case JAF_ARRAY_ALLOC:
		jaf_check_type(args->items[0], &int_type);
		break;
	case JAF_ARRAY_REALLOC:
		jaf_check_type(args->items[0], &int_type);
		break;
	case JAF_ARRAY_COPY:
		jaf_check_type(args->items[0], &int_type);
		// array lvalue of same type
		jaf_check_type(args->items[2], &int_type);
		jaf_check_type(args->items[3], &int_type);
		break;
	case JAF_ARRAY_FILL:
		jaf_check_type(args->items[0], &int_type);
		jaf_check_type(args->items[1], &int_type);
		// type of array data
		break;
	case JAF_ARRAY_PUSHBACK:
		// type of array data
		break;
	case JAF_ARRAY_ERASE:
		jaf_check_type(args->items[0], &int_type);
		break;
	case JAF_ARRAY_INSERT:
		jaf_check_type(args->items[0], &int_type);
		// type of array data
		break;
	case JAF_ARRAY_SORT:
		// comparator function
		break;
	case JAF_INT_STRING:
	case JAF_FLOAT_STRING:
	case JAF_STRING_INT:
	case JAF_STRING_LENGTH:
	case JAF_STRING_LENGTHBYTE:
	case JAF_STRING_EMPTY:
	case JAF_STRING_POPBACK:
	case JAF_ARRAY_FREE:
	case JAF_ARRAY_NUMOF:
	case JAF_ARRAY_POPBACK:
	case JAF_ARRAY_EMPTY:
		break;
	default:
		COMPILER_ERROR(expr, "Invalid builtin");
	}
}

static void jaf_check_types_call(struct jaf_env *env, struct jaf_expression *expr)
{
	switch ((int)expr->call.fun->valuetype.data) {
	case AIN_FUNCTION:
		jaf_check_types_function_call(env, expr);
		break;
	case AIN_FUNC_TYPE:
		jaf_check_types_functype_call(env, expr);
		break;
	case AIN_SYSCALL:
		jaf_check_types_system_call(env, expr);
		break;
	case AIN_HLLCALL:
		jaf_check_types_hll_call(env, expr, NULL);
		break;
	case AIN_METHOD:
		jaf_check_types_method_call(env, expr);
		break;
	case AIN_SUPER:
		jaf_check_types_super_call(env, expr);
		break;
	case AIN_BUILTIN:
		if (expr->call.fun->member.object_no < 0) {
			jaf_check_types_builtin_call(env, expr);
		} else {
			jaf_check_types_builtin_hll_call(env, expr);
		}
		break;
	default:
		JAF_ERROR(expr, "Invalid expression in function position");
	}
}

static void jaf_check_types_new(struct jaf_env *env, struct jaf_expression *expr)
{
	if (expr->new.type->type != JAF_STRUCT)
		TYPE_ERROR(expr, AIN_STRUCT);

	// FIXME: polymorphism
	int struct_no = expr->new.type->struct_no;
	assert(struct_no >= 0 && struct_no < env->ain->nr_structures);
	expr->new.func_no = env->ain->structures[struct_no].constructor;
	if (expr->new.func_no <= 0) {
		// Some constructors are not listed in the struct definition, so
		// we have to look them up by function name
		struct string *method_name = make_method_name(env->ain, struct_no, "0");
		expr->new.func_no = ain_get_function(env->ain, method_name->text);
		free_string(method_name);
	}

	if (expr->new.func_no < 0) {
		if (expr->new.args->nr_items > 0) {
			JAF_ERROR(expr, "Too many arguments to (default) constructor");
		}
	} else {
		jaf_check_function_arguments(expr, expr->new.args, &env->ain->functions[expr->new.func_no]);
	}

	expr->valuetype.data = AIN_REF_STRUCT;
	expr->valuetype.struc = struct_no;
}

static int get_member_no(struct ain *ain, int struct_type, const char *_name)
{
	int member_no = -1;
	char *name = conv_output(_name);
	struct ain_struct *s = &ain->structures[struct_type];
	for (int i = 0; i < s->nr_members; i++) {
		if (!strcmp(s->members[i].name, name)) {
			member_no = i;
			break;
		}
	}
	free(name);
	return member_no;
}

static int get_method_no(struct ain *ain, int struct_type, const char *name)
{
	// TODO: polymorphism
	struct string *method_name = make_method_name(ain, struct_type, name);
	int method_no = ain_get_function(ain, method_name->text);
	free_string(method_name);
	return method_no;
}

static int get_library_function_no(struct ain *ain, int lib_no, const char *_name)
{
	char *name = conv_output(_name);
	int func_no = ain_get_library_function(ain, lib_no, name);
	free(name);
	return func_no;
}

static int get_system_call_no(possibly_unused struct ain *ain, const char *name)
{
	for (int i = 0; i < NR_SYSCALLS; i++) {
		if (!strcmp(name, syscalls[i].name+7)) {
			return i;
		}
	}
	return -1;
}

static enum jaf_builtin_method get_builtin_no(enum ain_data_type type, const char *name)
{
	for (int i = 0; i < JAF_NR_BUILTINS; i++) {
		if (builtins[i].type == type && !strcmp(name, builtins[i].name))
			return i;
	}
	return -1;
}

static void jaf_check_types_hll_builtin(struct jaf_env *env, struct jaf_expression *expr)
{
	struct jaf_expression *obj = expr->member.struc;
	const char *name = expr->member.name->text;
	int lib_no = get_builtin_lib(env->ain, obj->valuetype.data, expr);
	int no = get_library_function_no(env->ain, lib_no, name);
	if (no >= 0) {
		expr->valuetype.data = AIN_BUILTIN;
		expr->member.object_no = lib_no;
		expr->member.member_no = no;
	} else {
		JAF_ERROR(expr, "Undefined built-in method: %s.%s", env->ain->libraries[lib_no].name, name);
	}
}

static void jaf_check_types_member(struct jaf_env *env, struct jaf_expression *expr)
{
	struct jaf_expression *obj = expr->member.struc;
	const char *name = expr->member.name->text;
	switch ((int)obj->valuetype.data) {
	case AIN_STRUCT:
	case AIN_REF_STRUCT: {
		int struct_type = obj->valuetype.struc;
		expr->member.object_no = struct_type;
		assert(struct_type >= 0);
		assert(struct_type < env->ain->nr_structures);
		int no;
		if ((no = get_member_no(env->ain, struct_type, name)) >= 0) {
			struct ain_struct *s = &env->ain->structures[struct_type];
			expr->valuetype = s->members[no].type;
			expr->member.member_no = no;
		} else if ((no = get_method_no(env->ain, struct_type, name)) >= 0) {
			expr->valuetype.data = AIN_METHOD;
			expr->member.member_no = no;
		} else {
			// FIXME: show struct name
			JAF_ERROR(expr, "Invalid struct member name: %s", name);
		}
		break;
	}
	case AIN_INT:
	case AIN_REF_INT: {
		if (AIN_VERSION_GTE(env->ain, 8, 0)) {
			jaf_check_types_hll_builtin(env, expr);
			break;
		}
		if ((expr->member.member_no = get_builtin_no(AIN_INT, name)) < 0) {
			JAF_ERROR(expr, "Invalid integer builtin: %s", name);
		}
		expr->valuetype.data = AIN_BUILTIN;
		expr->member.object_no = JAF_BUILTIN_INT;
		break;
	}
	case AIN_FLOAT:
	case AIN_REF_FLOAT: {
		if (AIN_VERSION_GTE(env->ain, 8, 0)) {
			jaf_check_types_hll_builtin(env, expr);
			break;
		}
		if ((expr->member.member_no = get_builtin_no(AIN_FLOAT, name)) < 0) {
			JAF_ERROR(expr, "Invalid float builtin: %s", name);
		}
		expr->valuetype.data = AIN_BUILTIN;
		expr->member.object_no = JAF_BUILTIN_FLOAT;
		break;
	}
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE: {
		if ((expr->member.member_no = get_builtin_no(AIN_ARRAY, name)) < 0) {
			JAF_ERROR(expr, "Invalid array builtin: %s", name);
		}
		expr->valuetype.data = AIN_BUILTIN;
		expr->member.object_no = JAF_BUILTIN_ARRAY;
		break;
	}
	case AIN_STRING:
	case AIN_REF_STRING: {
		if (AIN_VERSION_GTE(env->ain, 8, 0)) {
			jaf_check_types_hll_builtin(env, expr);
			break;
		}
		if ((expr->member.member_no = get_builtin_no(AIN_STRING, name)) < 0) {
			JAF_ERROR(expr, "Invalid string builtin: %s", name);
		}
		expr->valuetype.data = AIN_BUILTIN;
		expr->member.object_no = JAF_BUILTIN_STRING;
		break;
	}
	case AIN_LIBRARY: {
		int lib_no = obj->valuetype.struc;
		assert(lib_no >= 0);
		assert(lib_no < env->ain->nr_libraries);
		int no = get_library_function_no(env->ain, obj->valuetype.struc, name);
		if (no >= 0) {
			expr->valuetype.data = AIN_HLLCALL;
			expr->member.object_no = lib_no;
			expr->member.member_no = no;
		} else {
			JAF_ERROR(expr, "Undefined HLL function: %s.%s", obj->s->text, name);
		}
		break;
	}
	case AIN_SYSTEM: {
		int no = get_system_call_no(env->ain, name);
		if (no >= 0) {
			expr->valuetype.data = AIN_SYSCALL;
			expr->member.member_no = no;
		} else {
			JAF_ERROR(expr, "Invalid system call: system.%s", name);
		}
		break;
	}
	case AIN_ARRAY:
	case AIN_REF_ARRAY: {
		jaf_check_types_hll_builtin(env, expr);
		break;
	}
	default:
		JAF_ERROR(expr, "Invalid expression as left side of dotted expression");
		break;
	}
}

/*
 * Get the data type of the elements in an array.
 */
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
	case AIN_ARRAY:           case AIN_REF_ARRAY:           return type->array_type->data;
	default:
		return AIN_VOID;
	}
}

/*
 * Get the full type of the elements in an array.
 */
static void array_deref_type(struct jaf_env *env, struct ain_type *dst, struct ain_type *src)
{
	if (src->rank > 1) {
		dst->data = src->data;
		dst->struc = src->struc;
		dst->rank = src->rank - 1;
	} else {
		assert(src->rank == 1);
		dst->data = array_data_type(src);
		dst->struc = src->struc;
		if (dst->data == AIN_STRUCT && AIN_VERSION_GTE(env->ain, 11, 0)) {
			dst->struc = src->array_type->struc;
		}
		dst->rank = 0;
	}
}

static void jaf_type_check_array(struct jaf_expression *expr)
{
	switch (expr->valuetype.data) {
	case AIN_ARRAY:
	case AIN_REF_ARRAY:
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE:
		return;
	default:
		TYPE_ERROR(expr, AIN_ARRAY);
	}
}

static void jaf_check_types_subscript(struct jaf_env *env, struct jaf_expression *expr)
{
	jaf_type_check_array(expr->subscript.expr);
	jaf_type_check_int(expr->subscript.index);
	array_deref_type(env, &expr->valuetype, &expr->subscript.expr->valuetype);
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
	case JAF_EXP_THIS:
		jaf_check_types_this(env, expr);
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
		jaf_check_types_call(env, expr);
		break;
	case JAF_EXP_NEW:
		jaf_check_types_new(env, expr);
		break;
	case JAF_EXP_CAST:
		expr->valuetype.data = jaf_to_ain_simple_type(expr->cast.type);
		break;
	case JAF_EXP_MEMBER:
		jaf_check_types_member(env, expr);
		break;
	case JAF_EXP_SEQ:
		expr->valuetype = expr->seq.tail->valuetype;
		break;
	case JAF_EXP_SUBSCRIPT:
		jaf_check_types_subscript(env, expr);
		break;
	case JAF_EXP_SYSCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_BUILTIN_CALL:
	case JAF_EXP_SUPER_CALL:
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
	case AIN_ENUM:
		return true;
	default:
		return false;
	}
}

void jaf_check_type(struct jaf_expression *expr, struct ain_type *type)
{
	if (type->data == AIN_VOID && !expr)
		return;
	if (!jaf_type_equal(&expr->valuetype, type)) {
		// numeric types are compatible (implicit cast)
		if (is_numeric(expr->valuetype.data) && is_numeric(type->data))
			return;
		if (type->data == AIN_FUNC_TYPE && expr->valuetype.data == AIN_FUNCTION) {
			// FIXME: check function signatures
			return;
		}
		TYPE_ERROR(expr, type->data);
	}
}
