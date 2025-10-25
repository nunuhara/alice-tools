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

static bool jaf_resolve_typedef(struct jaf_type_specifier *type, struct ain *ain)
{
	bool result = true;
	if (type->type == JAF_TYPEDEF) {
		int no;
		char *u = conv_output(type->name->text);
		if ((no = ain_get_struct(ain, u)) >= 0) {
			if (ain->structures[no].is_interface) {
				type->type = JAF_IFACE;
			} else {
				type->type = JAF_STRUCT;
			}
			type->struct_no = no;
		} else if ((no = ain_get_functype(ain, u)) >= 0) {
			type->type = JAF_FUNCTYPE;
			type->func_no = no;
		} else if ((no = ain_get_delegate(ain, u)) >= 0) {
			type->type = JAF_DELEGATE;
			type->func_no = no;
		} else if ((no = ain_get_enum(ain, u)) >= 0) {
			type->type = JAF_ENUM;
			type->struct_no = no;
		} else {
			result = false;
		}
		free(u);
	} else if (type->type == JAF_ARRAY) {
		result = jaf_resolve_typedef(type->array_type, ain);
	}
	return result;
}

static void jaf_resolve_typedef_e(struct jaf_expression *e, struct jaf_type_specifier *type, struct ain *ain)
{
	if (!jaf_resolve_typedef(type, ain))
		COMPILER_ERROR(e, "Failed to resolve typedef \"%s\"", type->name->text);
}

static void jaf_resolve_typedef_s(struct jaf_block_item *s, struct jaf_type_specifier *type, struct ain *ain)
{
	if (!jaf_resolve_typedef(type, ain))
		COMPILER_ERROR(s, "Failed to resolve typedef \"%s\"", type->name->text);
}

static struct jaf_expression *jaf_resolve_expression_types(struct jaf_expression *expr, struct jaf_visitor *visitor)
{
	struct ain *ain = visitor->data;

	switch (expr->type) {
	case JAF_EXP_NEW:
		jaf_resolve_typedef_e(expr, expr->new.type, ain);
		break;
	case JAF_EXP_VOID:
	case JAF_EXP_INT:
	case JAF_EXP_FLOAT:
	case JAF_EXP_STRING:
	case JAF_EXP_IDENTIFIER:
	case JAF_EXP_THIS:
	case JAF_EXP_UNARY:
	case JAF_EXP_BINARY:
	case JAF_EXP_TERNARY:
	case JAF_EXP_FUNCALL:
	case JAF_EXP_SYSCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_INTERFACE_CALL:
	case JAF_EXP_BUILTIN_CALL:
	case JAF_EXP_SUPER_CALL:
	case JAF_EXP_CAST:
	case JAF_EXP_MEMBER:
	case JAF_EXP_SEQ:
	case JAF_EXP_SUBSCRIPT:
	case JAF_EXP_CHAR:
	case JAF_EXP_NULL:
	case JAF_EXP_DUMMYREF:
		break;
	}

	return expr;
}

static void jaf_resolve_statement_types(struct jaf_block_item *item, struct jaf_visitor *visitor)
{
	struct ain *ain = visitor->data;

	switch (item->kind) {
	case JAF_DECL_VAR:
		jaf_resolve_typedef_s(item, item->var.type, ain);
		break;
	case JAF_DECL_FUN:
		jaf_resolve_typedef_s(item, item->fun.type, ain);
		break;
	case JAF_DECL_STRUCT:
	case JAF_DECL_INTERFACE:
	case JAF_DECL_FUNCTYPE:
	case JAF_DECL_DELEGATE:
	case JAF_STMT_NULL:
	case JAF_STMT_LABELED:
	case JAF_STMT_COMPOUND:
	case JAF_STMT_EXPRESSION:
	case JAF_STMT_IF:
	case JAF_STMT_SWITCH:
	case JAF_STMT_WHILE:
	case JAF_STMT_DO_WHILE:
	case JAF_STMT_FOR:
	case JAF_STMT_GOTO:
	case JAF_STMT_CONTINUE:
	case JAF_STMT_BREAK:
	case JAF_STMT_RETURN:
	case JAF_STMT_CASE:
	case JAF_STMT_DEFAULT:
	case JAF_STMT_MESSAGE:
	case JAF_STMT_RASSIGN:
	case JAF_STMT_ASSERT:
	case JAF_EOF:
		break;
	}
}

void jaf_resolve_types(struct ain *ain, struct jaf_block *block)
{
	struct jaf_visitor visitor = {
		.visit_stmt_post = jaf_resolve_statement_types,
		.visit_expr_post = jaf_resolve_expression_types,
		.data = ain,
	};
	jaf_accept_block(ain, block, &visitor);
}
