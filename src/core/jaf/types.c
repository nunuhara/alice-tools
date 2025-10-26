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

static enum jaf_type ain_to_jaf_numeric_type(enum ain_data_type t)
{
	switch (t) {
	case AIN_INT: return JAF_INT;
	case AIN_BOOL: return JAF_BOOL;
	case AIN_FLOAT: return JAF_FLOAT;
	case AIN_LONG_INT: return JAF_LONG_INT;
	default: _COMPILER_ERROR(NULL, -1, "Unhandled data type");
	}
}

static struct ain_variable *jaf_global_lookup(struct jaf_env *env, const char *name, int *var_no)
{
	int no = ain_get_global(env->ain, name);
	if (no < 0)
		return NULL;
	*var_no = no;
	return &env->ain->globals[no];
}

static enum ain_data_type _strip_ref(struct ain_type *type)
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

static struct ain_type strip_ref(struct ain_type *type)
{
	struct ain_type r = *type;
	r.data = _strip_ref(type);
	return r;
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

static void maybe_deref(struct jaf_expression *e)
{
	if (e->type == JAF_EXP_IDENTIFIER || e->type == JAF_EXP_MEMBER) {
		e->valuetype.data = _strip_ref(&e->valuetype);
	}
}

static bool type_identical(struct ain_type *a, struct ain_type *b)
{
	if (a->data != b->data)
		return false;
	switch ((int)a->data) {
	case AIN_ARRAY_STRUCT:
	case AIN_REF_ARRAY_STRUCT:
	case AIN_ARRAY_FUNC_TYPE:
	case AIN_REF_ARRAY_FUNC_TYPE:
	case AIN_ARRAY_DELEGATE:
	case AIN_REF_ARRAY_DELEGATE:
		if (a->rank != b->rank)
			return false;
		// fallthrough
	case AIN_STRUCT:
	case AIN_REF_STRUCT:
	case AIN_IFACE:
	case AIN_FUNC_TYPE:
	case AIN_REF_FUNC_TYPE:
	case AIN_DELEGATE:
	case AIN_REF_DELEGATE:
		if (a->struc != b->struc)
			return false;
		break;
	case AIN_ARRAY:
	case AIN_REF_ARRAY:
	case AIN_WRAP:
		assert(a->rank == 1 && a->array_type);
		assert(b->rank == 1 && b->array_type);
		return type_identical(a->array_type, b->array_type);
	default:
		break;
	}
	return true;
}

static bool iface_compatible(struct jaf_env *env, int iface_no, int struct_no)
{
	assert(struct_no >= 0 && struct_no < env->ain->nr_structures);
	struct ain_struct *s = &env->ain->structures[struct_no];
	for (int i = 0; i < s->nr_interfaces; i++) {
		if (s->interfaces[i].struct_type == iface_no)
			return true;
	}
	return false;
}

static bool type_equal(struct jaf_env *env, struct ain_type *expected, struct ain_type *actual)
{
	if (expected->data == actual->data) {
		switch (expected->data) {
		case AIN_ARRAY_STRUCT:
		case AIN_REF_ARRAY_STRUCT:
		case AIN_ARRAY_FUNC_TYPE:
		case AIN_REF_ARRAY_FUNC_TYPE:
		case AIN_ARRAY_DELEGATE:
		case AIN_REF_ARRAY_DELEGATE:
			if (expected->rank != actual->rank)
				return false;
			// fallthrough
		case AIN_STRUCT:
		case AIN_REF_STRUCT:
		case AIN_IFACE:
		case AIN_FUNC_TYPE:
		case AIN_REF_FUNC_TYPE:
		case AIN_DELEGATE:
		case AIN_REF_DELEGATE:
			return expected->struc == -1 || expected->struc == actual->struc;
		case AIN_ARRAY:
		case AIN_REF_ARRAY:
		case AIN_WRAP:
			// XXX: array without array type means any array type allowed
			//      (used by Array HLL)
			if (!expected->rank)
				return true;
			assert(actual->rank == 1 && actual->array_type);
			return type_equal(env, expected->array_type, actual->array_type);
		default:
			return true;
		}
	}
#define T(a,b) (((a) << 8) | (b))
	switch (T(expected->data, actual->data)) {
	case T(AIN_INT, AIN_BOOL):
	case T(AIN_INT, AIN_LONG_INT):
		return true;
	case T(AIN_BOOL, AIN_INT):
	case T(AIN_BOOL, AIN_LONG_INT):
		return true;
	case T(AIN_LONG_INT, AIN_INT):
	case T(AIN_LONG_INT, AIN_BOOL):
		return true;
	case T(AIN_IMAIN_SYSTEM, AIN_INT):
		return true;
	case T(AIN_NULLTYPE, AIN_FUNC_TYPE):
	case T(AIN_NULLTYPE, AIN_DELEGATE):
	case T(AIN_NULLTYPE, AIN_IMAIN_SYSTEM):
		return true;
	case T(AIN_IFACE, AIN_STRUCT):
		return iface_compatible(env, expected->struc, actual->struc);
	default:
		return false;
	}
#undef T
}

static void type_check(struct jaf_env *env, struct ain_type *expected, struct jaf_expression *actual)
{
	if (ain_is_ref_data_type(expected->data)) {
		maybe_deref(actual);
	}

	switch ((int)actual->valuetype.data) {
	case AIN_NULLTYPE:
		switch (expected->data) {
		case AIN_REF_TYPE:
		case AIN_FUNC_TYPE:
		case AIN_DELEGATE:
		case AIN_IMAIN_SYSTEM:
			ain_copy_type(&actual->valuetype, expected);
			break;
		default:
			TYPE_ERROR(actual, expected->data);
		}
		break;
	default:
		if (!type_equal(env, expected, &actual->valuetype))
			TYPE_ERROR(actual, expected->data);
		break;
	}
}

static void ref_type_check(struct jaf_env *env, struct ain_type *expected, struct jaf_expression *actual)
{
	if ((int)actual->valuetype.data == AIN_NULLTYPE) {
		ain_copy_type(&actual->valuetype, expected);
		actual->valuetype.data = add_ref(expected);
		return;
	}
	struct ain_type actual_t = strip_ref(&actual->valuetype);
	if (!type_equal(env, expected, &actual_t))
		TYPE_ERROR(actual, actual_t.data);

	if (expected->data == AIN_IFACE && actual_t.data == AIN_STRUCT) {
		// cast to interface
		struct jaf_expression *copy = xmalloc(sizeof(struct jaf_expression));
		*copy = *actual;
		actual->type = JAF_EXP_CAST;
		actual->cast.type = JAF_IFACE;
		actual->cast.expr = copy;
		ain_copy_type(&actual->valuetype, expected);
	}
}

static enum ain_data_type type_check_numeric(struct jaf_expression *e)
{
	maybe_deref(e);
	switch ((int)e->valuetype.data) {
	case AIN_INT:
	case AIN_BOOL:
	case AIN_LONG_INT:
	case AIN_FLOAT:
		return e->valuetype.data;
	default:
		TYPE_ERROR(e, e->valuetype.data);
	}
}

static void coerce_cast(enum ain_data_type t, struct jaf_expression *e)
{
	struct jaf_expression *copy = xmalloc(sizeof(struct jaf_expression));
	*copy = *e;
	e->type = JAF_EXP_CAST;
	e->cast.type = ain_to_jaf_numeric_type(t);
	e->cast.expr = copy;
	e->valuetype.data = t;
}

static void cast_to_method(struct jaf_expression *e)
{
	struct jaf_expression *copy = xmalloc(sizeof(struct jaf_expression));
	*copy = *e;
	e->type = JAF_EXP_CAST;
	e->cast.type = JAF_FUNCTYPE;
	e->cast.expr = copy;
	e->valuetype.data = AIN_METHOD;
	e->valuetype.struc = copy->valuetype.struc;
}

static enum ain_data_type type_coerce_numerics(struct jaf_expression *parent,
		enum jaf_operator op, struct jaf_expression *a, struct jaf_expression *b)
{
#define CAST(v, t) ( \
	coerce_cast(AIN_##t, v), \
	v->valuetype.data \
)
	type_check_numeric(a);
	type_check_numeric(b);

	enum ain_data_type a_t = a->valuetype.data;
	enum ain_data_type b_t = b->valuetype.data;

	if (a_t == b_t) {
		if (a_t == AIN_BOOL) {
			switch (op) {
			case JAF_EQ:
			case JAF_NEQ:
			case JAF_LOG_AND:
			case JAF_LOG_OR:
			case JAF_BIT_AND:
			case JAF_BIT_IOR:
			case JAF_BIT_XOR:
				return AIN_BOOL;
			default:
				JAF_ERROR(parent, "invalid operation on boolean type");
			}
		}
		return a_t;
	}

	if (a_t == AIN_FLOAT)
		return CAST(b, FLOAT);
	if (b_t == AIN_FLOAT)
		return CAST(a, FLOAT);
	if ((a_t == AIN_LONG_INT && b_t == AIN_LONG_INT) || (a_t == AIN_INT && b_t == AIN_LONG_INT)) {
		switch (op) {
		case JAF_EQ:
		case JAF_NEQ:
		case JAF_LT:
		case JAF_GT:
		case JAF_LTE:
		case JAF_GTE:
			return AIN_LONG_INT;
		default:
			break;
		}
	}
	if (a_t == AIN_LONG_INT)
		return CAST(b, LONG_INT);
	if (b_t == AIN_LONG_INT)
		return CAST(a, LONG_INT);
	if (a_t == AIN_INT)
		return CAST(b, INT);
	if (b_t == AIN_INT)
		return CAST(a, INT);
	COMPILER_ERROR(parent, "coerce_numerics: non-numeric type");
#undef CAST
}


static void check_lvalue(struct jaf_expression *e)
{
	switch (e->type) {
	case JAF_EXP_IDENTIFIER:
	case JAF_EXP_MEMBER:
		if (e->valuetype.data == AIN_FUNCTION)
			JAF_ERROR(e, "Invalid expression as lvalue");
		break;
	case JAF_EXP_SUBSCRIPT:
	case JAF_EXP_NEW:
		break;
	default:
		JAF_ERROR(e, "Invalid expression as lvalue");
	}
}

static void check_referenceable(struct jaf_expression *e)
{
	switch ((int)e->valuetype.data) {
	case AIN_NULLTYPE:
	case AIN_REF_TYPE:
	case AIN_IFACE:
		break;
	default:
		if (e->type != JAF_EXP_THIS)
			check_lvalue(e);
		break;
	}
}

static struct ain_function_type functype_of_fundecl(struct ain_function *f)
{
	return (struct ain_function_type) {
		.name = f->name,
		.return_type = f->return_type,
		.nr_arguments = f->nr_args,
		.nr_variables = f->nr_vars,
		.variables = f->vars
	};
}

static bool functype_compatible(struct ain_function_type *a, struct ain_function_type *b)
{
	if (!type_identical(&a->return_type, &b->return_type))
		return false;
	if (a->nr_arguments != b->nr_arguments)
		return false;
	for (int i = 0; i < a->nr_arguments; i++) {
		if (!type_identical(&a->variables[i].type, &b->variables[i].type))
			return false;
	}
	return true;
}

static struct ain_function_type *get_delegate(struct jaf_env *env, int no)
{
	assert(no >= 0 && no < env->ain->nr_delegates);
	return &env->ain->delegates[no];
}

static struct ain_function_type *get_functype(struct jaf_env *env, int no)
{
	assert(no >= 0 && no < env->ain->nr_function_types);
	return &env->ain->function_types[no];
}

static struct ain_function_type get_function(struct jaf_env *env, int no)
{
	assert(no >= 0 && no < env->ain->nr_functions);
	return functype_of_fundecl(&env->ain->functions[no]);
}

static struct ain_function_type get_method(struct jaf_env *env, int no, int *struct_no)
{
	assert(no >= 0 && no < env->ain->nr_functions);
	*struct_no = env->ain->functions[no].struct_type;
	return functype_of_fundecl(&env->ain->functions[no]);

}

static void check_delegate_compatible(struct jaf_env *env, struct ain_type *t, struct jaf_expression *rhs)
{
	assert(t->struc >= 0 && t->struc < env->ain->nr_delegates);
	struct ain_function_type *dg = get_delegate(env, t->struc);
	switch ((int)rhs->valuetype.data) {
	case AIN_METHOD: {
		int struct_no;
		struct ain_function_type m = get_method(env, rhs->valuetype.struc, &struct_no);
		if (!functype_compatible(dg, &m))
			TYPE_ERROR(rhs, t->data);
		break;
	}
	case AIN_FUNCTION:
		struct ain_function_type f = get_function(env, rhs->valuetype.struc);
		if (!functype_compatible(dg, &f))
			TYPE_ERROR(rhs, t->data);
		cast_to_method(rhs);
		break;
	case AIN_DELEGATE: {
		if (rhs->valuetype.struc != t->struc)
			TYPE_ERROR(rhs, t->data);
		break;
	}
	default:
		TYPE_ERROR(rhs, t->data);
	}
}

/*
 * Used to check:
 *   - variable assignment (incl. initvals)
 *   - return values
 *   - function call arguments
 */
static void check_assign(struct jaf_env *env, struct ain_type *t, struct jaf_expression *rhs)
{
	switch ((int)t->data) {
	// Assigning to a functype or delegate variable is special. The RHS
	// should be an expression like &foo, which has type 'ref function'.
	// This is then converted into the declared functype of the variable
	// (if the prototypes match).
	case AIN_FUNC_TYPE: {
		struct ain_function_type *ft = get_functype(env, t->struc);
		switch ((int)rhs->valuetype.data) {
		case AIN_FUNCTION: {
			struct ain_function_type ft2 = get_function(env, rhs->valuetype.struc);
			if (!functype_compatible(ft, &ft2))
				TYPE_ERROR(rhs, t->data);
			break;
		}
		case AIN_FUNC_TYPE: {
			struct ain_function_type *ft2 = get_functype(env, rhs->valuetype.struc);
			if (!functype_compatible(ft, ft2))
				TYPE_ERROR(rhs, t->data);
			break;
		}
		case AIN_STRING:
			break;
		case AIN_NULLTYPE:
			ain_copy_type(&rhs->valuetype, t);
			break;
		default:
			TYPE_ERROR(rhs, t->data);
		}
		break;
	}
	case AIN_DELEGATE:
		check_delegate_compatible(env, t, rhs);
		break;
	case AIN_FUNCTION:
		if (rhs->valuetype.data == AIN_FUNCTION) {
			struct ain_function_type f = get_function(env, t->struc);
			struct ain_function_type f2 = get_function(env, rhs->valuetype.struc);
			if (!functype_compatible(&f, &f2))
				TYPE_ERROR(rhs, t->data);
		} else {
			TYPE_ERROR(rhs, t->data);
		}
		break;
	case AIN_METHOD:
		if (rhs->valuetype.data == AIN_METHOD) {
			int s, s2;
			struct ain_function_type f = get_method(env, t->struc, &s);
			struct ain_function_type f2 = get_method(env, rhs->valuetype.struc, &s2);
			if (s != s2 || !functype_compatible(&f, &f2))
				TYPE_ERROR(rhs, t->data);
		} else if (rhs->valuetype.data == AIN_FUNCTION) {
			struct ain_function_type f = get_function(env, t->struc);
			struct ain_function_type f2 = get_function(env, rhs->valuetype.struc);
			if (!functype_compatible(&f, &f2))
				TYPE_ERROR(rhs, t->data);
			cast_to_method(rhs);
		} else {
			TYPE_ERROR(rhs, t->data);
		}
		break;
	// TODO: interface methods?
	case AIN_INT:
	case AIN_LONG_INT:
	case AIN_BOOL:
	case AIN_FLOAT:
		if (type_check_numeric(rhs) != t->data) {
			coerce_cast(t->data, rhs);
		}
		break;
	case AIN_STRUCT:
		if (rhs->valuetype.data == AIN_REF_STRUCT && rhs->valuetype.struc == t->struc)
			/* nothing */;
		else
			type_check(env, t, rhs);
		break;
	default:
		type_check(env, t, rhs);
		break;
	}
}

static void check_ref_assign(struct jaf_env *env, struct jaf_expression *lhs, struct jaf_expression *rhs)
{
	// rhs must be a ref, or an lvalue in order to create a reference to it
	check_referenceable(rhs);
	maybe_deref(rhs);
	// check that lhs is a reference variable of the appropriate type
	switch (lhs->type) {
	case JAF_EXP_IDENTIFIER: {
		struct ain_type lhs_t;
		switch (lhs->ident.kind) {
		case JAF_IDENT_LOCAL:
			lhs_t = lhs->ident.local->valuetype;
			break;
		case JAF_IDENT_GLOBAL:
			assert(lhs->ident.global >= 0 && lhs->ident.global < env->ain->nr_globals);
			lhs_t = env->ain->globals[lhs->ident.global].type;
			break;
		case JAF_IDENT_CONST:
			// FIXME: should this be allowed?
			JAF_ERROR(lhs, "Reference assignment to const variable");
		case JAF_IDENT_UNRESOLVED:
			COMPILER_ERROR(lhs, "Unresolved identifier");
		}
		if (!ain_is_ref_data_type(lhs_t.data))
			JAF_ERROR(lhs, "Reference assignment to non-reference type");
		lhs_t = strip_ref(&lhs_t);
		ref_type_check(env, &lhs_t, rhs);
		break;
	}
	case JAF_EXP_MEMBER:
		if (!ain_is_ref_data_type(lhs->valuetype.data))
			JAF_ERROR(lhs, "Reference assignment to non-reference type");
		struct ain_type t = strip_ref(&lhs->valuetype);
		ref_type_check(env, &t, rhs);
		break;
	default:
		JAF_ERROR(lhs, "Invalid lvalue for reference assignment");
		break;
	}
}

static void type_check_identifier(struct jaf_env *env, struct jaf_expression *expr)
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
			expr->ident.kind = JAF_IDENT_CONST;
			expr->ident.constval = local->val;
		} else {
			ain_copy_type(&expr->valuetype, &local->decl->valuetype);
			expr->ident.kind = JAF_IDENT_LOCAL;
			expr->ident.local = local->decl;
		}
	} else if ((v = jaf_global_lookup(env, u, &no))) {
		ain_copy_type(&expr->valuetype, &v->type);
		expr->ident.kind = JAF_IDENT_GLOBAL;
		expr->ident.global = no;
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

static void type_check_this(struct jaf_env *env, struct jaf_expression *expr)
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

static struct ain_type int_type = { .data = AIN_INT };
static struct ain_type string_type = { .data = AIN_STRING };

static void type_check_unary(struct jaf_env *env, struct jaf_expression *expr)
{
	switch (expr->op) {
	case JAF_UNARY_PLUS:
	case JAF_UNARY_MINUS:
		type_check_numeric(expr->expr);
		expr->valuetype.data = expr->expr->valuetype.data;
		break;
	case JAF_PRE_INC:
	case JAF_PRE_DEC:
	case JAF_POST_INC:
	case JAF_POST_DEC:
		check_lvalue(expr->expr);
		type_check_numeric(expr->expr);
		expr->valuetype.data = expr->expr->valuetype.data;
		break;
	case JAF_BIT_NOT:
	case JAF_LOG_NOT:
		type_check(env, &int_type, expr->expr);
		expr->valuetype.data = AIN_INT;
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

static void check_function_argument(struct jaf_env *env, struct ain_type *t,
		struct jaf_expression *arg);

static void type_check_property_assign(struct jaf_env *env, struct jaf_expression *expr)
{
	int setter_no = expr->lhs->member.setter_no;
	assert(setter_no >= 0 && setter_no < env->ain->nr_functions);
	struct ain_function *f = &env->ain->functions[setter_no];
	check_function_argument(env, &f->vars[0].type, expr->rhs);
}

static void type_check_assign(struct jaf_env *env, struct jaf_expression *expr)
{
	check_lvalue(expr->lhs);
	switch (expr->op) {
	case JAF_ASSIGN:
		if (expr->lhs->type == JAF_EXP_MEMBER && expr->lhs->member.type == JAF_DOT_PROPERTY) {
			type_check_property_assign(env, expr);
			break;
		}
		check_assign(env, &expr->lhs->valuetype, expr->rhs);
		// if lhs is a string subscript access, change the operator to JAF_CHAR_ASSIGN
		if (expr->lhs->type == JAF_EXP_SUBSCRIPT) {
			enum ain_data_type a_t = expr->lhs->subscript.expr->valuetype.data;
			if (a_t == AIN_STRING || a_t == AIN_REF_STRING) {
				expr->op = JAF_CHAR_ASSIGN;
			}
		}
		break;
	case JAF_ADD_ASSIGN:
	case JAF_SUB_ASSIGN:
	case JAF_MUL_ASSIGN:
	case JAF_DIV_ASSIGN:
		if (expr->lhs->valuetype.data == AIN_STRING && expr->op == JAF_ADD_ASSIGN) {
			type_check(env, &string_type, expr->rhs);
		} else if (expr->lhs->valuetype.data == AIN_DELEGATE
				&& (expr->op == JAF_ADD_ASSIGN || expr->op == JAF_SUB_ASSIGN)) {
			check_delegate_compatible(env, &expr->lhs->valuetype, expr->rhs);
		} else {
			type_check_numeric(expr->lhs);
			type_check_numeric(expr->rhs);
			if (expr->rhs->valuetype.data != expr->lhs->valuetype.data) {
				// cast
				enum jaf_type jaf_t = ain_to_jaf_numeric_type(expr->lhs->valuetype.data);
				expr->rhs = jaf_cast_expression(jaf_t, expr->rhs);
				expr->rhs->valuetype.data = expr->lhs->valuetype.data;
			}
			type_check(env, &expr->lhs->valuetype, expr->rhs);
		}
		break;
	case JAF_MOD_ASSIGN:
	case JAF_LSHIFT_ASSIGN:
	case JAF_RSHIFT_ASSIGN:
	case JAF_AND_ASSIGN:
	case JAF_XOR_ASSIGN:
	case JAF_OR_ASSIGN:
		type_check(env, &int_type, expr->lhs);
		type_check(env, &int_type, expr->rhs);
		break;
	default:
		COMPILER_ERROR(expr, "Unhandled assignment operator");
	}
	// XXX: Nothing is left on stack after assigning method to delegate
	if (expr->lhs->valuetype.data == AIN_DELEGATE
			&& (expr->rhs->valuetype.data == AIN_METHOD
				|| expr->rhs->valuetype.data == AIN_STRING)) {
		expr->valuetype.data = AIN_VOID;
	} else {
		ain_copy_type(&expr->valuetype, &expr->rhs->valuetype);
	}
}

static void type_check_binary(struct jaf_env *env, struct jaf_expression *expr)
{
	if (expr->op != JAF_REQ && expr->op != JAF_RNE) {
		maybe_deref(expr->lhs);
		maybe_deref(expr->rhs);
	}
	switch (expr->op) {
	case JAF_PLUS:
		if (expr->lhs->valuetype.data == AIN_STRING) {
			type_check(env, &string_type, expr->rhs);
			ain_copy_type(&expr->valuetype, &expr->lhs->valuetype);
		} else {
			expr->valuetype.data = type_coerce_numerics(expr, expr->op,
					expr->lhs, expr->rhs);
		}
		break;
	case JAF_MINUS:
	case JAF_MULTIPLY:
	case JAF_DIVIDE:
		expr->valuetype.data = type_coerce_numerics(expr, expr->op, expr->lhs, expr->rhs);
		break;
	case JAF_REMAINDER:
		switch (expr->lhs->valuetype.data) {
		case AIN_STRING:
			// TODO: check type matches format specifier if format string is a literal
			switch (expr->rhs->valuetype.data) {
			case AIN_INT:
			case AIN_FLOAT:
			case AIN_BOOL:
			case AIN_LONG_INT:
			case AIN_STRING:
				break;
			default:
				JAF_ERROR(expr->rhs, "Invalid rhs for string formatting operator");
			}
			break;
		case AIN_INT:
		case AIN_LONG_INT:
			type_check(env, &int_type, expr->rhs);
			break;
		default:
			TYPE_ERROR(expr->lhs, AIN_INT);
		}
		expr->valuetype.data = expr->lhs->valuetype.data;
		break;
	case JAF_LSHIFT:
	case JAF_RSHIFT:
	case JAF_BIT_AND:
	case JAF_BIT_IOR:
	case JAF_BIT_XOR:
	case JAF_LOG_AND:
	case JAF_LOG_OR:
		type_check(env, &int_type, expr->lhs);
		type_check(env, &int_type, expr->rhs);
		expr->valuetype.data = AIN_INT;
		break;
	case JAF_EQ:
	case JAF_NEQ:
		// NOTE: NULL is not allowed on lhs
		if (expr->lhs->valuetype.data == AIN_STRING) {
			type_check(env, &string_type, expr->rhs);
		} else if (expr->lhs->valuetype.data == AIN_FUNC_TYPE) {
			switch ((int)expr->rhs->valuetype.data) {
			case AIN_NULLTYPE:
				ain_copy_type(&expr->rhs->valuetype, &expr->lhs->valuetype);
				break;
			case AIN_FUNCTION: {
				struct ain_function_type *ft = get_functype(env, expr->lhs->valuetype.struc);
				struct ain_function_type f = get_function(env, expr->rhs->valuetype.struc);
				if (!functype_compatible(ft, &f))
					TYPE_ERROR(expr->rhs, AIN_FUNC_TYPE);
				break;
			}
			case AIN_FUNC_TYPE:
				if (expr->lhs->valuetype.struc != expr->rhs->valuetype.struc)
					TYPE_ERROR(expr->rhs, AIN_FUNC_TYPE);
				break;
			default:
				TYPE_ERROR(expr->rhs, AIN_FUNC_TYPE);
			}
		} else {
			type_coerce_numerics(expr, expr->op, expr->lhs, expr->rhs);
		}
		expr->valuetype.data = AIN_INT;
		break;
	case JAF_LT:
	case JAF_GT:
	case JAF_LTE:
	case JAF_GTE:
		if (expr->lhs->valuetype.data == AIN_STRING) {
			type_check(env, &string_type, expr->rhs);
		} else {
			type_coerce_numerics(expr, expr->op, expr->lhs, expr->rhs);
		}
		expr->valuetype.data = AIN_INT;
		break;
	case JAF_REQ:
	case JAF_RNE:
		if (expr->lhs->type == JAF_EXP_IDENTIFIER || expr->lhs->type == JAF_EXP_MEMBER) {
			check_ref_assign(env, expr->lhs, expr->rhs);
		} else if (expr->lhs->type == JAF_EXP_THIS) {
			JAF_ERROR(expr->lhs, "Not an lvalue");
		} else {
			check_referenceable(expr->rhs);
			if (ain_is_ref_data_type(expr->lhs->valuetype.data)) {
				ref_type_check(env, &expr->lhs->valuetype, expr->rhs);
			} else {
				JAF_ERROR(expr->lhs, "Not an lvalue");
			}
		}
		expr->valuetype.data = AIN_INT;
		break;
	case JAF_ASSIGN:
	case JAF_ADD_ASSIGN:
	case JAF_SUB_ASSIGN:
	case JAF_MUL_ASSIGN:
	case JAF_DIV_ASSIGN:
	case JAF_MOD_ASSIGN:
	case JAF_LSHIFT_ASSIGN:
	case JAF_RSHIFT_ASSIGN:
	case JAF_AND_ASSIGN:
	case JAF_XOR_ASSIGN:
	case JAF_OR_ASSIGN:
		type_check_assign(env, expr);
		break;
	default:
		COMPILER_ERROR(expr, "Unhandled binary operator: %d", expr->op);
	}
}

static void type_check_ternary(struct jaf_env *env, struct jaf_expression *expr)
{
	maybe_deref(expr->consequent);
	maybe_deref(expr->alternative);
	type_check(env, &int_type, expr->condition);
	type_check(env, &expr->consequent->valuetype, expr->alternative);
	ain_copy_type(&expr->valuetype, &expr->consequent->valuetype);
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

static void check_function_argument(struct jaf_env *env, struct ain_type *t,
		struct jaf_expression *arg)
{
	switch (t->data) {
	case AIN_REF_TYPE: {
		struct ain_type strip_t = strip_ref(t);
		check_referenceable(arg);
		ref_type_check(env, &strip_t, arg);
		break;
	}
	default:
		check_assign(env, t, arg);
		break;
	}
}

static void check_function_arguments(struct jaf_env *env, struct jaf_expression *expr,
		struct jaf_argument_list *args, struct ain_function *f)
{
	int arg = 0;

	args->var_nos = xcalloc(args->nr_items, sizeof(int));
	for (unsigned i = 0; i < args->nr_items; i++, arg++) {
		if (arg >= f->nr_args)
			JAF_ERROR(expr, "Too many arguments to function %s", conv_utf8(f->name));

		check_function_argument(env, &f->vars[arg].type, args->items[i]);

		args->var_nos[i] = arg;
		if (ain_wide_type(f->vars[arg].type.data))
			arg++;
	}
	if (arg != f->nr_args)
		JAF_ERROR(expr, "Too few arguments to function %s", conv_utf8(f->name));
}

static void check_functype_arguments(struct jaf_env *env, struct jaf_expression *expr,
		struct jaf_argument_list *args, struct ain_function_type *f)
{
	int arg = 0;

	args->var_nos = xcalloc(args->nr_items, sizeof(int));
	for (unsigned i = 0; i < args->nr_items; i++, arg++) {
		if (arg >= f->nr_arguments)
			JAF_ERROR(expr, "Too many arguments to function type %s", conv_utf8(f->name));

		check_function_argument(env, &f->variables[arg].type, args->items[i]);

		args->var_nos[i] = arg;
		if (ain_wide_type(f->variables[arg].type.data))
			arg++;
	}
	if (arg != f->nr_arguments)
		JAF_ERROR(expr, "Too few arguments to function type %s", conv_utf8(f->name));
}

static void type_check_function_call(struct jaf_env *env, struct jaf_expression *expr)
{
	int func_no = expr->call.fun->valuetype.struc;
	assert(func_no >= 0 && func_no < env->ain->nr_functions);

	check_function_arguments(env, expr, expr->call.args, &env->ain->functions[func_no]);
	ain_copy_type(&expr->valuetype, &env->ain->functions[func_no].return_type);
	expr->call.func_no = func_no;
}

static void type_check_functype_call(struct jaf_env *env, struct jaf_expression *expr)
{
	int ft_no = expr->call.fun->valuetype.struc;
	assert(ft_no >= 0 && ft_no < env->ain->nr_function_types);

	check_functype_arguments(env, expr, expr->call.args, &env->ain->function_types[ft_no]);
	ain_copy_type(&expr->valuetype, &env->ain->function_types[ft_no].return_type);
	expr->call.func_no = ft_no;
}

static void type_check_delegate_call(struct jaf_env *env, struct jaf_expression *expr)
{
	int dg_no = expr->call.fun->valuetype.struc;
	assert(dg_no >= 0 && dg_no < env->ain->nr_delegates);

	check_functype_arguments(env, expr, expr->call.args, &env->ain->delegates[dg_no]);
	ain_copy_type(&expr->valuetype, &env->ain->delegates[dg_no].return_type);
	expr->call.func_no = dg_no;
}

static void type_check_system_call(struct jaf_env *env, struct jaf_expression *expr)
{
	expr->type = JAF_EXP_SYSCALL;
	expr->call.func_no = expr->call.fun->member.member_no;

	for (size_t i = 0; i < expr->call.args->nr_items; i++) {
		struct ain_type type = {
			.data = syscalls[expr->call.func_no].argtypes[i],
			.struc = -1,
			.rank = 0
		};
		check_function_argument(env, &type, expr->call.args->items[i]);
	}
	ain_copy_type(&expr->valuetype, &syscalls[expr->call.func_no].return_type);
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

static void check_hll_argument(struct jaf_env *env, struct jaf_expression *arg, struct ain_type *type, struct ain_type *type_param)
{
	if (type && (type->data == AIN_HLL_PARAM || type->data == AIN_REF_HLL_PARAM)) {
		if (type_param->data != AIN_ARRAY && type_param->data != AIN_REF_ARRAY)
			COMPILER_ERROR(arg, "Expected array as type param");
		type_check(env, type_param->array_type, arg);
	} else if (!AIN_VERSION_GTE(env->ain, 14, 0)
			&& (type->data == AIN_STRUCT || type->data == AIN_REF_STRUCT)) {
		// XXX: special case since hll types are data-only (until v14+)
		if (arg->valuetype.data != AIN_STRUCT) {
			TYPE_ERROR(arg, type->data);
		}
	} else {
		check_function_argument(env, type, arg);
	}
}

static void type_check_hll_call(struct jaf_env *env, struct jaf_expression *expr,
		struct ain_type *type)
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
		check_hll_argument(env, expr->call.args->items[i], &def->arguments[i].type, type);
	}

	if (def->return_type.data == AIN_HLL_PARAM) {
		ain_copy_type(&expr->valuetype, type->array_type);
	} else if (def->return_type.data == AIN_REF_HLL_PARAM) {
		ain_copy_type(&expr->valuetype, type->array_type);
		expr->valuetype.data = add_ref(&expr->valuetype);
	} else if (def->return_type.data == AIN_ARRAY) {
		if (!strcmp(obj_name, "String") && !strcmp(mbr_name, "Split")) {
			expr->valuetype.data = AIN_ARRAY;
			expr->valuetype.rank = 1;
			expr->valuetype.array_type = xcalloc(1, sizeof(struct ain_type));
			expr->valuetype.array_type->data = AIN_STRING;
		}
		if (!strcmp(obj_name, "FileOperation") &&
				(!strcmp(mbr_name, "GetFileList") ||
				 !strcmp(mbr_name, "GetFileListWithSubFolder") ||
				 !strcmp(mbr_name, "GetFolderList"))) {
			expr->valuetype.data = AIN_ARRAY;
			expr->valuetype.rank = 1;
			expr->valuetype.array_type = xcalloc(1, sizeof(struct ain_type));
			expr->valuetype.array_type->data = AIN_STRING;
		}
	} else {
		ain_copy_type(&expr->valuetype, &def->return_type);
	}
}

static void type_check_method_call(struct jaf_env *env, struct jaf_expression *expr)
{
	struct jaf_expression *obj = expr->call.fun->member.struc;
	assert(obj->valuetype.data == AIN_STRUCT || obj->valuetype.data == AIN_REF_STRUCT);
	assert(obj->valuetype.struc >= 0);

	int method_no = expr->call.fun->member.member_no;
	expr->type = JAF_EXP_METHOD_CALL;
	expr->call.func_no = method_no;
	check_function_arguments(env, expr, expr->call.args, &env->ain->functions[method_no]);
	ain_copy_type(&expr->valuetype, &env->ain->functions[method_no].return_type);
}

static void type_check_interface_call(struct jaf_env *env, struct jaf_expression *expr)
{
	struct jaf_expression *obj = expr->call.fun->member.struc;
	assert(obj->valuetype.data == AIN_IFACE);
	assert(obj->valuetype.struc >= 0 && obj->valuetype.struc < env->ain->nr_structures);

	int method_no = expr->call.fun->member.member_no;
	expr->type = JAF_EXP_INTERFACE_CALL;
	expr->call.func_no = method_no;

	struct ain_struct *s = &env->ain->structures[obj->valuetype.struc];
	assert(method_no >= 0 && method_no < s->nr_iface_methods);
	check_functype_arguments(env, expr, expr->call.args, &s->iface_methods[method_no]);
	ain_copy_type(&expr->valuetype, &s->iface_methods[method_no].return_type);
}

static void type_check_super_call(struct jaf_env *env, struct jaf_expression *expr)
{
	int method_no = expr->call.fun->valuetype.struc;
	expr->type = JAF_EXP_SUPER_CALL;
	expr->call.func_no = method_no;
	check_function_arguments(env, expr, expr->call.args, &env->ain->functions[method_no]);
	ain_copy_type(&expr->valuetype, &env->ain->functions[method_no].return_type);
}

static const char *_builtin_type_name(enum ain_data_type type)
{
	switch (type) {
	case AIN_INT: return "Int";
	case AIN_FLOAT: return "Float";
	case AIN_STRING: return "String";
	case AIN_ARRAY: return "Array";
	case AIN_DELEGATE: return "Delegate";
	default: _COMPILER_ERROR(NULL, -1, "Invalid type for builtin");
	}
}

static struct string *builtin_type_name(enum ain_data_type type)
{
	const char *name = _builtin_type_name(type);
	return make_string(name, strlen(name));
}

struct builtin {
	enum ain_data_type return_type;
	enum ain_data_type type;
	const char *name;
	int min_args;
	int max_args;
	enum opcode opcode;
};

static struct builtin builtins[] = {
	[JAF_INT_STRING]        = { AIN_STRING, AIN_INT, "String",        0, 0, I_STRING },
	[JAF_FLOAT_STRING]      = { AIN_STRING, AIN_FLOAT, "String",      0, 0, FTOS },
	[JAF_STRING_INT]        = { AIN_INT,    AIN_STRING, "Int",        0, 0, STOI },
	[JAF_STRING_LENGTH]     = { AIN_INT,    AIN_STRING, "Length",     0, 0, S_LENGTH },
	[JAF_STRING_LENGTHBYTE] = { AIN_INT,    AIN_STRING, "LengthByte", 0, 0, S_LENGTHBYTE },
	[JAF_STRING_EMPTY]      = { AIN_INT,    AIN_STRING, "Empty",      0, 0, S_EMPTY },
	[JAF_STRING_FIND]       = { AIN_INT,    AIN_STRING, "Find",       1, 1, S_FIND },
	[JAF_STRING_GETPART]    = { AIN_STRING, AIN_STRING, "GetPart",    2, 2, S_GETPART },
	[JAF_STRING_PUSHBACK]   = { AIN_VOID,   AIN_STRING, "PushBack",   1, 1, S_PUSHBACK },
	[JAF_STRING_POPBACK]    = { AIN_VOID,   AIN_STRING, "PopBack",    0, 0, S_POPBACK },
	[JAF_STRING_ERASE]      = { AIN_VOID,   AIN_STRING, "Erase",      1, 1, S_ERASE },
	[JAF_ARRAY_ALLOC]       = { AIN_VOID,   AIN_ARRAY, "Alloc",       1, 255, A_ALLOC },
	[JAF_ARRAY_REALLOC]     = { AIN_VOID,   AIN_ARRAY, "Realloc",     1, 1, A_REALLOC },
	[JAF_ARRAY_FREE]        = { AIN_VOID,   AIN_ARRAY, "Free",        0, 0, A_FREE },
	[JAF_ARRAY_NUMOF]       = { AIN_INT,    AIN_ARRAY, "Numof",       0, 1, A_NUMOF },
	[JAF_ARRAY_COPY]        = { AIN_INT,    AIN_ARRAY, "Copy",        4, 4, A_COPY },
	[JAF_ARRAY_FILL]        = { AIN_INT,    AIN_ARRAY, "Fill",        3, 3, A_FILL },
	[JAF_ARRAY_PUSHBACK]    = { AIN_VOID,   AIN_ARRAY, "PushBack",    1, 1, A_PUSHBACK },
	[JAF_ARRAY_POPBACK]     = { AIN_VOID,   AIN_ARRAY, "PopBack",     0, 0, A_POPBACK },
	[JAF_ARRAY_EMPTY]       = { AIN_INT,    AIN_ARRAY, "Empty",       0, 0, A_EMPTY },
	[JAF_ARRAY_ERASE]       = { AIN_INT,    AIN_ARRAY, "Erase",       1, 1, A_ERASE },
	[JAF_ARRAY_INSERT]      = { AIN_VOID,   AIN_ARRAY, "Insert",      2, 2, A_INSERT },
	[JAF_ARRAY_SORT]        = { AIN_VOID,   AIN_ARRAY, "Sort",        0, 1, A_SORT },
	[JAF_ARRAY_FIND]        = { AIN_INT,    AIN_ARRAY, "Find",        3, 4, A_FIND },
	[JAF_DELEGATE_NUMOF]    = { AIN_INT,    AIN_DELEGATE, "Numof",    0, 0, DG_NUMOF },
	[JAF_DELEGATE_EXIST]    = { AIN_INT,    AIN_DELEGATE, "Exist",    1, 1, DG_EXIST },
	[JAF_DELEGATE_CLEAR]    = { AIN_VOID,   AIN_DELEGATE, "Clear",    0, 0, DG_CLEAR },
};

static enum ain_data_type array_data_type(struct ain_type *type);

static void jaf_check_comparator_type(struct jaf_env *env, struct jaf_expression *expr,
		struct ain_type *val_type)
{
	if (expr->valuetype.data != AIN_FUNCTION)
		JAF_ERROR(expr, "Type error (expected comparator; got %s)",
				ain_strtype_d(env->ain, &expr->valuetype));
	assert(expr->valuetype.struc >= 0 && expr->valuetype.struc < env->ain->nr_functions);
	struct ain_function *f = &env->ain->functions[expr->valuetype.struc];
	if (f->nr_args != 2)
		JAF_ERROR(expr, "Wrong number of arguments to comparator (expected 2; got %d)",
				f->nr_args);
	if (val_type->data == AIN_STRUCT) {
		if (f->vars[0].type.data != AIN_REF_STRUCT
				|| f->vars[1].type.data != AIN_REF_STRUCT
				|| f->vars[0].type.struc != val_type->struc
				|| f->vars[1].type.struc != val_type->struc)
			JAF_ERROR(expr, "Wrong argument type for comparator");
	} else if (f->vars[0].type.data != val_type->data
			|| f->vars[1].type.data != val_type->data) {
		JAF_ERROR(expr, "Wrong argument type for comparator");
	}
}

static void type_check_builtin_call(struct jaf_env *env, struct jaf_expression *expr)
{
	struct builtin *builtin = &builtins[expr->call.fun->member.member_no];
	if (expr->call.args->nr_items < builtin->min_args)
		JAF_ERROR(expr, "Too few arguments to builtin method: %s.%s",
				_builtin_type_name(builtin->type), builtin->name);
	if (expr->call.args->nr_items > builtin->max_args)
		JAF_ERROR(expr, "Too many arguments to builtin method: %s.%s",
				_builtin_type_name(builtin->type), builtin->name);

	expr->type = JAF_EXP_BUILTIN_CALL;
	expr->valuetype.data = builtin->return_type;
	expr->call.func_no = builtin->opcode;

	struct ain_type val_type = { .data = AIN_VOID };
	if (builtin->type == AIN_ARRAY) {
		val_type.data = array_data_type(&expr->call.fun->member.struc->valuetype);
		val_type.struc = expr->call.fun->member.struc->valuetype.struc;
	}
	struct jaf_argument_list *args = expr->call.args;
	switch ((enum jaf_builtin_method)expr->call.fun->member.member_no) {
	case JAF_STRING_FIND:
		type_check(env, &string_type, args->items[0]);
		break;
	case JAF_STRING_GETPART:
		type_check(env, &int_type, args->items[0]);
		type_check(env, &int_type, args->items[1]);
		break;
	case JAF_STRING_PUSHBACK:
		type_check(env, &int_type, args->items[0]);
		break;
	case JAF_STRING_ERASE:
		type_check(env, &int_type, args->items[0]);
		break;
	case JAF_ARRAY_ALLOC:
		for (int i = 0; i < args->nr_items; i++) {
			type_check(env, &int_type, args->items[i]);
		}
		// check nr args matches array rank
		break;
	case JAF_ARRAY_REALLOC:
		for (int i = 0; i < args->nr_items; i++) {
			type_check(env, &int_type, args->items[i]);
		}
		// check nr args matches array rank
		break;
	case JAF_ARRAY_NUMOF:
		if (args->nr_items > 0)
			type_check(env, &int_type, args->items[0]);
		break;
	case JAF_ARRAY_COPY:
		type_check(env, &int_type, args->items[0]);
		type_check(env, &expr->call.fun->member.struc->valuetype, args->items[1]);
		type_check(env, &int_type, args->items[2]);
		type_check(env, &int_type, args->items[3]);
		break;
	case JAF_ARRAY_FILL:
		type_check(env, &int_type, args->items[0]);
		type_check(env, &int_type, args->items[1]);
		type_check(env, &val_type, args->items[2]);
		break;
	case JAF_ARRAY_PUSHBACK:
		type_check(env, &val_type, args->items[0]);
		break;
	case JAF_ARRAY_ERASE:
		type_check(env, &int_type, args->items[0]);
		break;
	case JAF_ARRAY_INSERT:
		type_check(env, &int_type, args->items[0]);
		type_check(env, &val_type, args->items[1]);
		break;
	case JAF_ARRAY_SORT:
		if (args->nr_items > 0) {
			jaf_check_comparator_type(env, args->items[0], &val_type);
		}
		break;
	case JAF_ARRAY_FIND:
		type_check(env, &int_type, args->items[0]);
		type_check(env, &int_type, args->items[1]);
		type_check(env, &val_type, args->items[2]);
		if (args->nr_items > 3) {
			jaf_check_comparator_type(env, args->items[3], &val_type);
		}
		break;
	case JAF_DELEGATE_EXIST:
		check_delegate_compatible(env, &expr->call.fun->member.struc->valuetype, args->items[0]);
		break;
	case JAF_INT_STRING:
	case JAF_FLOAT_STRING:
	case JAF_STRING_INT:
	case JAF_STRING_LENGTH:
	case JAF_STRING_LENGTHBYTE:
	case JAF_STRING_EMPTY:
	case JAF_STRING_POPBACK:
	case JAF_ARRAY_FREE:
	case JAF_ARRAY_POPBACK:
	case JAF_ARRAY_EMPTY:
	case JAF_DELEGATE_NUMOF:
	case JAF_DELEGATE_CLEAR:
		break;
	default:
		COMPILER_ERROR(expr, "Invalid builtin");
	}
}

/*
 * NOTE: We rewrite the expression here into a regular HLL call to avoid duplicated code.
 */
static void type_check_builtin_hll_call(struct jaf_env *env, struct jaf_expression *expr)
{
	struct jaf_expression *obj = expr->call.fun->member.struc;
	// dummy expression
	expr->call.fun->member.struc = jaf_identifier(builtin_type_name(obj->valuetype.data));

	// put obj at head of argument list
	struct jaf_argument_list *args = expr->call.args;
	args->items = xrealloc_array(args->items, args->nr_items, args->nr_items+1,
			sizeof(struct jaf_expression*));
	for (unsigned i = args->nr_items; i > 0; i--) {
		args->items[i] = args->items[i-1];
	}
	args->items[0] = obj;
	args->nr_items++;

	type_check_hll_call(env, expr, &obj->valuetype);
}

static void type_check_call(struct jaf_env *env, struct jaf_expression *expr)
{
	switch ((int)expr->call.fun->valuetype.data) {
	case AIN_FUNCTION:
		type_check_function_call(env, expr);
		break;
	case AIN_FUNC_TYPE:
		type_check_functype_call(env, expr);
		break;
	case AIN_DELEGATE:
		type_check_delegate_call(env, expr);
		break;
	case AIN_SYSCALL:
		type_check_system_call(env, expr);
		break;
	case AIN_HLLCALL:
		type_check_hll_call(env, expr, NULL);
		break;
	case AIN_METHOD:
		type_check_method_call(env, expr);
		break;
	case AIN_IMETHOD:
		type_check_interface_call(env, expr);
		break;
	case AIN_SUPER:
		type_check_super_call(env, expr);
		break;
	case AIN_BUILTIN:
		if (expr->call.fun->member.object_no < 0) {
			type_check_builtin_call(env, expr);
		} else {
			type_check_builtin_hll_call(env, expr);
		}
		break;
	default:
		JAF_ERROR(expr, "Invalid expression in function position");
	}
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

static void type_check_new(struct jaf_env *env, struct jaf_expression *expr)
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
		check_function_arguments(env, expr, expr->new.args,
				&env->ain->functions[expr->new.func_no]);
	}

	expr->valuetype.data = AIN_REF_STRUCT;
	expr->valuetype.struc = struct_no;
}

static void type_check_cast(struct jaf_env *env, struct jaf_expression *expr)
{
	maybe_deref(expr->cast.expr);

	switch (expr->cast.type) {
	case JAF_INT:
	case JAF_LONG_INT:
	case JAF_BOOL:
	case JAF_FLOAT:
	case JAF_STRING:
		break;
	default:
		JAF_ERROR(expr, "Invalid cast");
	}
	switch (expr->cast.expr->valuetype.data) {
	case AIN_INT:
	case AIN_LONG_INT:
	case AIN_BOOL:
	case AIN_FLOAT:
		break;
	default:
		JAF_ERROR(expr, "Invalid cast");
	}

	expr->valuetype.data = jaf_to_ain_simple_type(expr->cast.type);
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

static int get_interface_method_no(struct ain *ain, int iface_no, const char *_name)
{
	int member_no = -1;
	char *name = conv_output(_name);
	struct ain_struct *s = &ain->structures[iface_no];
	for (int i = 0; i < s->nr_iface_methods; i++) {
		if (!strcmp(s->iface_methods[i].name, name)) {
			member_no = i;
			break;
		}
	}
	free(name);
	return member_no;
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

static int get_property(struct ain *ain, int struct_no, const char *_name,
		int *getter_out, int *setter_out)
{
	char *name = conv_output(_name);
	size_t name_len = strlen(name);
	struct ain_struct *s = &ain->structures[struct_no];

	// "<Property>"
	struct string *property_name = make_string("<", 1);
	string_append_cstr(&property_name, name, name_len);
	string_push_back(&property_name, '>');

	// check that property exists
	int property_no = -1;
	for (int i = 0; i < s->nr_members; i++) {
		if (!strcmp(s->members[i].name, property_name->text)) {
			property_no = i;
			break;
		}
	}
	free_string(property_name);
	if (property_no < 0) {
		free(name);
		return -1;
	}

	// "S@Property::get" / "S@Property::set"
	struct string *getter_name = make_string(s->name, strlen(s->name));
	string_push_back(&getter_name, '@');
	string_append_cstr(&getter_name, name, name_len);
	string_push_back(&getter_name, ':');
	string_push_back(&getter_name, ':');
	struct string *setter_name = string_dup(getter_name);
	string_append_cstr(&getter_name, "get", 3);
	string_append_cstr(&setter_name, "set", 3);

	int getter_no = ain_get_function(ain, getter_name->text);
	int setter_no = ain_get_function(ain, setter_name->text);
	free_string(getter_name);
	free_string(setter_name);
	free(name);
	if (getter_no < 0 || setter_no < 0) {
		return -1;
	}

	// TODO: check function types

	*getter_out = getter_no;
	*setter_out = setter_no;
	return property_no;
}

static void type_check_member(struct jaf_env *env, struct jaf_expression *expr)
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
		int getter, setter;
		if ((no = get_member_no(env->ain, struct_type, name)) >= 0) {
			struct ain_struct *s = &env->ain->structures[struct_type];
			ain_copy_type(&expr->valuetype, &s->members[no].type);
			expr->member.member_no = no;
			expr->member.type = JAF_DOT_MEMBER;
		} else if ((no = get_method_no(env->ain, struct_type, name)) >= 0) {
			expr->valuetype.data = AIN_METHOD;
			expr->valuetype.struc = no;
			expr->member.member_no = no;
			expr->member.type = JAF_DOT_METHOD;
		} else if ((no = get_property(env->ain, struct_type, name, &getter, &setter)) >= 0) {
			struct ain_variable *property = &env->ain->structures[struct_type].members[no];
			ain_copy_type(&expr->valuetype, &property->type);
			expr->member.getter_no = getter;
			expr->member.setter_no = setter;
			expr->member.type = JAF_DOT_PROPERTY;
		} else {
			// FIXME: show struct name
			JAF_ERROR(expr, "Invalid struct member name: %s", name);
		}
		break;
	}
	case AIN_IFACE: {
		int iface_no = obj->valuetype.struc;
		expr->member.object_no = iface_no;
		assert(iface_no >= 0 && iface_no < env->ain->nr_structures);
		expr->member.member_no = get_interface_method_no(env->ain, iface_no, name);
		if (expr->member.member_no < 0) {
			JAF_ERROR(expr, "Invalid interface method name: %s", name);
		}
		expr->valuetype.data = AIN_IMETHOD;
		expr->valuetype.struc = expr->member.member_no;
		break;
	}
	case AIN_INT:
	case AIN_REF_INT:
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
	case AIN_FLOAT:
	case AIN_REF_FLOAT:
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
	case AIN_ARRAY_TYPE:
	case AIN_REF_ARRAY_TYPE:
		if ((expr->member.member_no = get_builtin_no(AIN_ARRAY, name)) < 0) {
			JAF_ERROR(expr, "Invalid array builtin: %s", name);
		}
		expr->valuetype.data = AIN_BUILTIN;
		expr->member.object_no = JAF_BUILTIN_ARRAY;
		break;
	case AIN_STRING:
	case AIN_REF_STRING:
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
	case AIN_DELEGATE:
	case AIN_REF_DELEGATE:
		if (AIN_VERSION_GTE(env->ain, 8, 0)) {
			jaf_check_types_hll_builtin(env, expr);
			break;
		}
		if ((expr->member.member_no = get_builtin_no(AIN_DELEGATE, name)) < 0) {
			JAF_ERROR(expr, "Invalid delegate builtin: %s", name);
		}
		expr->valuetype.data = AIN_BUILTIN;
		expr->member.object_no = JAF_BUILTIN_DELEGATE;
		break;
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
	case AIN_REF_ARRAY:
		jaf_check_types_hll_builtin(env, expr);
		break;
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
		assert(AIN_VERSION_LT(env->ain, 11, 0));
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

static void type_check_subscript(struct jaf_env *env, struct jaf_expression *expr)
{
	maybe_deref(expr->subscript.expr);
	type_check(env, &int_type, expr->subscript.index);
	if (expr->subscript.expr->valuetype.data == AIN_STRING) {
		expr->valuetype.data = AIN_INT;
	} else {
		switch (expr->subscript.expr->valuetype.data) {
		case AIN_ARRAY_TYPE:
		case AIN_REF_ARRAY_TYPE:
		case AIN_ARRAY:
		case AIN_REF_ARRAY:
			array_deref_type(env, &expr->valuetype, &expr->subscript.expr->valuetype);
			break;
		default:
			TYPE_ERROR(expr->subscript.expr, AIN_ARRAY);
		}
	}
}

void jaf_type_check_expression(struct jaf_env *env, struct jaf_expression *expr)
{
	switch (expr->type) {
	case JAF_EXP_VOID:
		expr->valuetype.data = AIN_VOID;
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
		type_check_identifier(env, expr);
		break;
	case JAF_EXP_THIS:
		type_check_this(env, expr);
		break;
	case JAF_EXP_UNARY:
		type_check_unary(env, expr);
		break;
	case JAF_EXP_BINARY:
		type_check_binary(env, expr);
		break;
	case JAF_EXP_TERNARY:
		type_check_ternary(env, expr);
		break;
	case JAF_EXP_FUNCALL:
		type_check_call(env, expr);
		break;
	case JAF_EXP_NEW:
		type_check_new(env, expr);
		break;
	case JAF_EXP_CAST:
		type_check_cast(env, expr);
		break;
	case JAF_EXP_MEMBER:
		type_check_member(env, expr);
		break;
	case JAF_EXP_SEQ:
		ain_copy_type(&expr->valuetype, &expr->seq.tail->valuetype);
		break;
	case JAF_EXP_SUBSCRIPT:
		type_check_subscript(env, expr);
		break;
	case JAF_EXP_NULL:
		expr->valuetype.data = AIN_NULLTYPE;
		break;
	case JAF_EXP_DUMMYREF:
		ain_copy_type(&expr->valuetype, &expr->dummy.expr->valuetype);
		break;
	case JAF_EXP_SYSCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_INTERFACE_CALL:
	case JAF_EXP_BUILTIN_CALL:
	case JAF_EXP_SUPER_CALL:
		// these should be JAF_EXP_FUNCALLs initially
		JAF_ERROR(expr, "Unexpected expression type");
	}
}

static void analyze_array_allocation(possibly_unused struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	if (!decl->array_dims)
		return;
	for (size_t i = 0; i < decl->type->rank; i++) {
		if (decl->array_dims[i]->type != JAF_EXP_INT)
			JAF_ERROR(item, "Invalid expression as array size");
	}
}

static void type_check_initval(struct jaf_env *env, struct ain_type *t, struct jaf_expression *expr)
{
	if (ain_is_ref_data_type(t->data)) {
		struct ain_type strip_t = strip_ref(t);
		check_referenceable(expr);
		maybe_deref(expr);
		ref_type_check(env, &strip_t, expr);
	} else {
		check_assign(env, t, expr);
	}
}

static void jaf_to_initval(struct ain_initval *dst, struct ain *ain, struct jaf_expression *expr)
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
	case JAF_EXP_IDENTIFIER: {
		char *name = conv_output(expr->ident.name->text);
		int no = ain_get_global(ain, name);
		if (no < 0)
			JAF_ERROR(expr, "Unresolved identifier in initval");
		dst->data_type = AIN_INT;
		dst->int_value = no;
		free(name);
		break;
	}
	default:
		JAF_ERROR(expr, "Initval is not constant");
	}
}

static void analyze_const_declaration(struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	if (!decl->init) {
		JAF_ERROR(item, "const declaration without an initializer");
	}
	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);
	type_check_initval(env, &decl->valuetype, decl->init);

	env->locals = xrealloc_array(env->locals, env->nr_locals, env->nr_locals+2,
				     sizeof(struct jaf_env_local));
	env->locals[env->nr_locals].name = decl->name->text;
	env->locals[env->nr_locals].is_const = true;
	jaf_to_initval(&env->locals[env->nr_locals].val, env->ain, decl->init);
	env->nr_locals++;
}

static void analyze_global_declaration(struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	if (!decl->init)
		return;

	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);
	type_check_initval(env, &decl->valuetype, decl->init);

	// add initval to ain object
	int g_no = ain_get_global(env->ain, decl->name->text);
	assert(g_no >= 0);
	int no = ain_add_initval(env->ain, g_no);
	jaf_to_initval(&env->ain->global_initvals[no], env->ain, decl->init);
	analyze_array_allocation(env, item);
}

static void analyze_local_declaration(struct jaf_env *env, struct jaf_block_item *item)
{
	struct jaf_vardecl *decl = &item->var;
	jaf_to_ain_type(env->ain, &decl->valuetype, decl->type);
	if (decl->init) {
		type_check_initval(env, &decl->valuetype, decl->init);
	}
	analyze_array_allocation(env, item);
}

static void analyze_message(struct jaf_env *env, struct jaf_block_item *item)
{
	if (!item->msg.func) {
		item->msg.func_no = -1;
		return;
	}

	char *u = conv_output(item->msg.func->text);
	if ((item->msg.func_no = ain_get_function(env->ain, u)) < 0)
		JAF_ERROR(item, "Undefined function: %s", item->msg.func->text);
	free(u);
}

void jaf_type_check_vardecl(struct jaf_env *env, struct jaf_block_item *decl)
{
	assert(decl->kind == JAF_DECL_VAR);
	assert(decl->var.type);
	if (decl->var.type->qualifiers & JAF_QUAL_CONST) {
		analyze_const_declaration(env, decl);
	} else if (env->parent) {
		analyze_local_declaration(env, decl);
	} else {
		analyze_global_declaration(env, decl);
	}
}

void jaf_type_check_statement(struct jaf_env *env, struct jaf_block_item *stmt)
{
	switch (stmt->kind) {
	case JAF_STMT_MESSAGE:
		analyze_message(env, stmt);
		break;
	case JAF_STMT_RETURN: {
		assert(env->func_no >= 0 && env->func_no < env->ain->nr_functions);
		struct ain_type *rtype = &env->ain->functions[env->func_no].return_type;
		if (rtype->data == AIN_VOID && stmt->expr)
			JAF_ERROR(stmt, "Return with value in void function");
		if (rtype->data != AIN_VOID && !stmt->expr)
			JAF_ERROR(stmt, "Return without a value in non-void function");
		if (!stmt->expr)
			break;
		if (ain_is_ref_data_type(rtype->data)) {
			check_referenceable(stmt->expr);
			struct ain_type t = strip_ref(rtype);
			ref_type_check(env, &t, stmt->expr);
		} else {
			check_assign(env, rtype, stmt->expr);
		}
		break;
	}
	case JAF_STMT_RASSIGN:
		check_ref_assign(env, stmt->rassign.lhs, stmt->rassign.rhs);
		break;
	default:
		break;
	}
}
