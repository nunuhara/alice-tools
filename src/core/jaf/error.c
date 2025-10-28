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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice/jaf.h"
#include "alice/port.h"

static const char *jaf_op_to_string(enum jaf_operator op)
{
	switch (op) {
	case JAF_AMPERSAND: return "&";
	case JAF_UNARY_PLUS: return "+";
	case JAF_UNARY_MINUS: return "-";
	case JAF_BIT_NOT: return "~";
	case JAF_LOG_NOT: return "!";
	case JAF_PRE_INC: return "++";
	case JAF_PRE_DEC: return "--";
	case JAF_POST_INC: return "++";
	case JAF_POST_DEC: return "--";
	case JAF_MULTIPLY: return "*";
	case JAF_DIVIDE: return "/";
	case JAF_REMAINDER: return "%";
	case JAF_PLUS: return "+";
	case JAF_MINUS: return "-";
	case JAF_LSHIFT: return "<<";
	case JAF_RSHIFT: return ">>";
	case JAF_LT: return "<";
	case JAF_GT: return ">";
	case JAF_LTE: return "<=";
	case JAF_GTE: return ">=";
	case JAF_EQ: return "==";
	case JAF_NEQ: return "!=";
	case JAF_REQ: return "===";
	case JAF_RNE: return "!==";
	case JAF_BIT_AND: return "&";
	case JAF_BIT_XOR: return "^";
	case JAF_BIT_IOR: return "|";
	case JAF_LOG_AND: return "&&";
	case JAF_LOG_OR: return "||";
	case JAF_ASSIGN: return "=";
	case JAF_MUL_ASSIGN: return "*=";
	case JAF_DIV_ASSIGN: return "/=";
	case JAF_MOD_ASSIGN: return "%=";
	case JAF_ADD_ASSIGN: return "+=";
	case JAF_SUB_ASSIGN: return "-=";
	case JAF_LSHIFT_ASSIGN: return "<<=";
	case JAF_RSHIFT_ASSIGN: return ">>=";
	case JAF_AND_ASSIGN: return "&=";
	case JAF_XOR_ASSIGN: return "^=";
	case JAF_OR_ASSIGN: return "|=";
	case JAF_CHAR_ASSIGN: return "=";
	default: ERROR("Unhandled operator: %d", op);
	}
}

const char *jaf_type_to_string(enum jaf_type type)
{
	switch (type) {
	case JAF_VOID: return "void";
	case JAF_INT: return "int";
	case JAF_BOOL: return "bool";
	case JAF_FLOAT: return "float";
	case JAF_STRING: return "string";
	case JAF_STRUCT: return "struct";
	case JAF_IFACE: return "interface";
	case JAF_ENUM: return "enum";
	case JAF_ARRAY: return "array";
	case JAF_WRAP: return "wrap";
	case JAF_HLL_PARAM: return "hll_param";
	case JAF_HLL_FUNC_71: return "hll_func_71";
	case JAF_HLL_FUNC: return "hll_func";
	case JAF_DELEGATE: return "delegate";
	case JAF_TYPEDEF: return "typedef";
	case JAF_FUNCTYPE: return "functype";
	default: ERROR("Unhandled type: %d", type);
	}
}

void jaf_print_expression(struct port *out, struct jaf_expression *expr);

static void print_arglist(struct port *out, struct jaf_argument_list *list)
{
	port_putc(out, '(');
	if (list->nr_items) {
		for (size_t i = 0; i < list->nr_items; i++) {
			if (i > 0)
				port_printf(out, ", ");
			jaf_print_expression(out, list->items[i]);
		}
	}
	port_putc(out, ')');
}

static void print_type_specifier(struct port *out, struct jaf_type_specifier *type)
{
	if (type->qualifiers & JAF_QUAL_OVERRIDE) {
		port_printf(out, "override ");
	}
	if (type->qualifiers & JAF_QUAL_CONST) {
		port_printf(out, "const ");
	}
	if (type->qualifiers & JAF_QUAL_REF) {
		port_printf(out, "ref ");
	}
	if (type->type == JAF_STRUCT || type->type == JAF_IFACE || type->type == JAF_FUNCTYPE
			|| type->type == JAF_DELEGATE || type->type == JAF_ENUM) {
		port_printf(out, "%s", type->name->text);
	} else {
		port_printf(out, "%s", jaf_type_to_string(type->type));
	}

	if (type->type == JAF_ARRAY) {
		port_putc(out, '<');
		print_type_specifier(out, type->array_type);
		port_printf(out, ">@%u", type->rank);
	} else if (type->type == JAF_WRAP) {
		port_putc(out, '<');
		print_type_specifier(out, type->array_type);
		port_putc(out, '>');
	}
}

void jaf_print_expression(struct port *out, struct jaf_expression *expr)
{
	// TODO: eliminate parentheses when possible
	switch (expr->type) {
	case JAF_EXP_VOID:
		port_printf(out, "(void)");
		break;
	case JAF_EXP_INT:
		port_printf(out, "%d", expr->i);
		break;
	case JAF_EXP_FLOAT:
		port_printf(out, "%f", expr->f);
		break;
	case JAF_EXP_STRING:
		// FIXME: handle escapes
		port_printf(out, "\"%s\"", expr->s->text);
		break;
	case JAF_EXP_THIS:
		port_printf(out, "this");
		break;
	case JAF_EXP_IDENTIFIER:
		port_printf(out, "%s", expr->ident.name->text);
		break;
	case JAF_EXP_UNARY:
		port_putc(out, '(');
		if (expr->op == JAF_POST_INC || expr->op == JAF_POST_DEC) {
			jaf_print_expression(out, expr->expr);
			port_printf(out, "%s", jaf_op_to_string(expr->op));
		} else {
			port_printf(out, "%s", jaf_op_to_string(expr->op));
			jaf_print_expression(out, expr->expr);
		}
		port_putc(out, ')');
		break;
	case JAF_EXP_BINARY:
		port_putc(out, '(');
		jaf_print_expression(out, expr->lhs);
		port_putc(out, ' ');
		port_printf(out, "%s", jaf_op_to_string(expr->op));
		port_putc(out, ' ');
		jaf_print_expression(out, expr->rhs);
		port_putc(out, ')');
		break;
	case JAF_EXP_TERNARY:
		port_putc(out, '(');
		port_putc(out, '(');
		jaf_print_expression(out, expr->condition);
		port_putc(out, ')');
		port_printf(out, " ? (");
		jaf_print_expression(out, expr->consequent);
		port_printf(out, ") : (");
		jaf_print_expression(out, expr->alternative);
		port_putc(out, ')');
		port_putc(out, ')');
		break;
	case JAF_EXP_FUNCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_BUILTIN_CALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_INTERFACE_CALL:
	case JAF_EXP_SUPER_CALL:
		jaf_print_expression(out, expr->call.fun);
		print_arglist(out, expr->call.args);
		break;
	case JAF_EXP_SYSCALL:
		port_printf(out, "%s", syscalls[expr->call.func_no].name);
		print_arglist(out, expr->call.args);
		break;
	case JAF_EXP_NEW:
		port_printf(out, "(new ");
		print_type_specifier(out, expr->new.type);
		print_arglist(out, expr->new.args);
		port_printf(out, ")");
		break;
	case JAF_EXP_CAST:
		port_printf(out, "((%s) ", jaf_type_to_string(expr->cast.type));
		jaf_print_expression(out, expr->cast.expr);
		port_putc(out, ')');
		break;
	case JAF_EXP_MEMBER:
		jaf_print_expression(out, expr->member.struc);
		port_printf(out, ".%s", expr->member.name->text);
		break;
	case JAF_EXP_SEQ:
		port_putc(out, '(');
		jaf_print_expression(out, expr->seq.head);
		port_printf(out, ", ");
		jaf_print_expression(out, expr->seq.tail);
		port_putc(out, ')');
		break;
	case JAF_EXP_SUBSCRIPT:
		jaf_print_expression(out, expr->subscript.expr);
		port_putc(out, '[');
		jaf_print_expression(out, expr->subscript.index);
		port_putc(out, ']');
		break;
	case JAF_EXP_CHAR:
		port_printf(out, "'%s'", expr->s->text);
		break;
	case JAF_EXP_NULL:
		port_printf(out, "NULL");
		break;
	case JAF_EXP_DUMMYREF:
		jaf_print_expression(out, expr->dummy.expr);
		break;
	default:
		ERROR("Unhandled expression type: %d", expr->type);
	}
}

static void print_vardecl(struct port *out, struct jaf_vardecl *decl)
{
	print_type_specifier(out, decl->type);
	port_printf(out, " %s", decl->name->text);
	if (decl->array_dims) {
		for (unsigned i = 0; i < decl->type->rank; i++) {
			port_putc(out, '[');
			jaf_print_expression(out, decl->array_dims[i]);
			port_putc(out, ']');
		}
	}
	if (decl->init) {
		port_printf(out, " = ");
		jaf_print_expression(out, decl->init);
	}
}

static void print_fundecl(struct port *out, struct jaf_fundecl *decl)
{
	print_type_specifier(out, decl->type);
	port_printf(out, "%s(", jaf_name_collapse(NULL, &decl->name)->text);
	if (decl->params) {
		struct jaf_block *params = decl->params;
		for (size_t i = 0; i < params->nr_items; i++) {
			if (i > 0)
				port_printf(out, ", ");
			assert(params->items[i]->kind == JAF_DECL_VAR);
			print_vardecl(out, &params->items[i]->var);
		}
	}
	port_putc(out, ')');
}

void jaf_print_block_item(struct port *out, struct jaf_block_item *item)
{
	switch (item->kind) {
	case JAF_DECL_VAR:
		print_vardecl(out, &item->var);
		port_putc(out, ';');
		break;
	case JAF_DECL_FUNCTYPE:
		port_printf(out, "functype ");
		print_fundecl(out, &item->fun);
		break;
	case JAF_DECL_DELEGATE:
		port_printf(out, "delegate ");
		print_fundecl(out, &item->fun);
		break;
	case JAF_DECL_FUN:
		print_fundecl(out, &item->fun);
		port_printf(out, " { ... }");
		break;
	case JAF_DECL_STRUCT:
		port_printf(out, "struct %s { ... }", item->struc.name->text);
		break;
	case JAF_DECL_INTERFACE:
		port_printf(out, "interface %s { ... }", item->struc.name->text);
		break;
	case JAF_STMT_LABELED:
	        jaf_print_block_item(out, item->label.stmt);
		break;
	case JAF_STMT_COMPOUND:
		port_printf(out, "{ ... }");
		break;
	case JAF_STMT_EXPRESSION:
		jaf_print_expression(out, item->expr);
		port_putc(out, ';');
		break;
	case JAF_STMT_IF:
		port_printf(out, "if (");
		jaf_print_expression(out, item->cond.test);
		port_printf(out, ") ...");
		break;
	case JAF_STMT_SWITCH:
		port_printf(out, "switch (");
		jaf_print_expression(out, item->swi.expr);
		port_printf(out, ") ...");
		break;
	case JAF_STMT_WHILE:
		port_printf(out, "while (");
		jaf_print_expression(out, item->while_loop.test);
		port_printf(out, ") ...");
		break;
	case JAF_STMT_DO_WHILE:
		port_printf(out, "do { ... } while (");
		jaf_print_expression(out, item->while_loop.test);
		port_printf(out, ")");
		break;
	case JAF_STMT_FOR:
		port_printf(out, "for (");
		for (size_t i = 0; i < item->for_loop.init->nr_items; i++) {
			jaf_print_block_item(out, item->for_loop.init->items[i]);
		}
		port_putc(out, ';');
		if (item->for_loop.test) {
			port_putc(out, ' ');
			jaf_print_expression(out, item->for_loop.test);
		}
		port_putc(out, ';');
		if (item->for_loop.after) {
			port_putc(out, ' ');
			jaf_print_expression(out, item->for_loop.after);
		}
		port_printf(out, ") ...");
		break;
	case JAF_STMT_GOTO:
		port_printf(out, "goto ???;"); // TODO
		break;
	case JAF_STMT_CONTINUE:
		port_printf(out, "continue;");
		break;
	case JAF_STMT_BREAK:
		port_printf(out, "break;");
		break;
	case JAF_STMT_RETURN:
		port_printf(out, "return");
		if (item->expr) {
			port_putc(out, ' ');
			jaf_print_expression(out, item->expr);
		}
		port_putc(out, ';');
		break;
	case JAF_STMT_CASE:
		port_printf(out, "case ");
		jaf_print_expression(out, item->swi_case.expr);
		port_printf(out, ": ...");
		break;
	case JAF_STMT_DEFAULT:
		port_printf(out, "default: ...");
		break;
	case JAF_STMT_MESSAGE:
		port_printf(out, "'%s'", item->msg.text->text);
		if (item->msg.func) {
			port_printf(out, "%s", item->msg.func->text);
		}
		port_putc(out, ';');
		break;
	case JAF_STMT_RASSIGN:
		jaf_print_expression(out, item->rassign.lhs);
		port_printf(out, " <- ");
		jaf_print_expression(out, item->rassign.rhs);
		port_putc(out, ';');
		break;
	case JAF_STMT_ASSERT:
		port_printf(out, "assert(");
		jaf_print_expression(out, item->assertion.expr);
		port_printf(out, ");");
		break;
	case JAF_EOF:
		port_printf(out, "<<EOF>>");
		break;
	default:
		ERROR("Unhandled block item type: %d", item->kind);
	}
}

#define RED "\033[0;31m"
#define CYAN "\033[0;36m"
#define RESET "\033[0m"

static void jaf_error_msg(const char *file, int line, const char *msgf, va_list ap)
{
	sys_warning("%s:%d: " RED "error: " RESET, file, line);
	sys_vwarning(msgf, ap);
}


_Noreturn void jaf_generic_error(const char *file, int line, const char *msgf, ...)
{
	va_list ap;
	va_start(ap, msgf);
	jaf_error_msg(file, line, msgf, ap);
	va_end(ap);

	sys_warning("\n");
	sys_exit(1);
}

_Noreturn void jaf_expression_error(struct jaf_expression *expr, const char *msgf, ...)
{
	va_list ap;
	va_start(ap, msgf);
	jaf_error_msg(expr->file, expr->line, msgf, ap);
	va_end(ap);

	struct port out;
	port_file_init(&out, stderr);

	sys_warning(CYAN "\n\tin: " RESET);
	jaf_print_expression(&out, expr);
	sys_warning("\n");
	sys_exit(1);
}

_Noreturn void jaf_block_item_error(struct jaf_block_item *item, const char *msgf, ...)
{
	va_list ap;
	va_start(ap, msgf);
	jaf_error_msg(item->file, item->line, msgf, ap);
	va_end(ap);

	struct port out;
	port_file_init(&out, stderr);

	sys_warning(CYAN "\n\tin: " RESET);
	jaf_print_block_item(&out, item);
	sys_warning("\n");
	sys_exit(1);
}

struct string *jaf_expression_to_string(struct jaf_expression *expr)
{
	struct port out;
	port_buffer_init(&out);
	jaf_print_expression(&out, expr);

	struct string *str = make_string((const char*)out.buffer.buf, out.buffer.index);
	port_close(&out);
	return str;
}
