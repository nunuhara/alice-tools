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
#include "system4/instructions.h"
#include "system4/string.h"
#include "jaf.h"

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
	case JAF_REF_ASSIGN: return "->";
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
	case JAF_ENUM: return "enum";
	case JAF_ARRAY: return "array";
	case JAF_WRAP: return "wrap";
	case JAF_HLL_PARAM: return "hll_param";
	case JAF_HLL_FUNC: return "hll_func";
	case JAF_DELEGATE: return "delegate";
	case JAF_TYPEDEF: return "typedef";
	case JAF_FUNCTYPE: return "functype";
	default: ERROR("Unhandled type: %d", type);
	}
}

void jaf_print_expression(FILE *out, struct jaf_expression *expr);

static void print_arglist(FILE *out, struct jaf_argument_list *list)
{
	fputc('(', out);
	if (list->nr_items) {
		for (size_t i = 0; i < list->nr_items; i++) {
			if (i > 0)
				fprintf(out, ", ");
			jaf_print_expression(out, list->items[i]);
		}
	}
	fputc(')', out);
}

static void print_type_specifier(FILE *out, struct jaf_type_specifier *type)
{
	if (type->qualifiers & JAF_QUAL_OVERRIDE) {
		fprintf(out, "override ");
	}
	if (type->qualifiers & JAF_QUAL_CONST) {
		fprintf(out, "const ");
	}
	if (type->qualifiers & JAF_QUAL_REF) {
		fprintf(out, "ref ");
	}
	if (type->type == JAF_STRUCT || type->type == JAF_FUNCTYPE || type->type == JAF_ENUM) {
		fprintf(out, "%s", type->name->text);
	} else {
		fprintf(out, "%s", jaf_type_to_string(type->type));
	}

	if (type->type == JAF_ARRAY) {
		fputc('<', out);
		print_type_specifier(out, type->array_type);
		fprintf(out, ">@%u", type->rank);
	} else if (type->type == JAF_WRAP) {
		fputc('<', out);
		print_type_specifier(out, type->array_type);
		fputc('>', out);
	}
}

void jaf_print_expression(FILE *out, struct jaf_expression *expr)
{
	// TODO: eliminate parentheses when possible
	switch (expr->type) {
	case JAF_EXP_VOID:
		fprintf(out, "(void)");
		break;
	case JAF_EXP_INT:
		fprintf(out, "%d", expr->i);
		break;
	case JAF_EXP_FLOAT:
		fprintf(out, "%f", expr->f);
		break;
	case JAF_EXP_STRING:
		// FIXME: handle escapes
		fprintf(out, "\"%s\"", expr->s->text);
		break;
	case JAF_EXP_THIS:
		fprintf(out, "this");
		break;
	case JAF_EXP_IDENTIFIER:
		fprintf(out, "%s", expr->ident.name->text);
		break;
	case JAF_EXP_UNARY:
		fputc('(', out);
		if (expr->op == JAF_POST_INC || expr->op == JAF_POST_DEC) {
			jaf_print_expression(out, expr->expr);
			fprintf(out, "%s", jaf_op_to_string(expr->op));
		} else {
			fprintf(out, "%s", jaf_op_to_string(expr->op));
			jaf_print_expression(out, expr->expr);
		}
		fputc(')', out);
		break;
	case JAF_EXP_BINARY:
		fputc('(', out);
		jaf_print_expression(out, expr->lhs);
		fputc(' ', out);
		fprintf(out, jaf_op_to_string(expr->op));
		fputc(' ', out);
		jaf_print_expression(out, expr->rhs);
		fputc(')', out);
		break;
	case JAF_EXP_TERNARY:
		fputc('(', out);
		fputc('(', out);
		jaf_print_expression(out, expr->condition);
		fputc(')', out);
		fprintf(out, " ? (");
		jaf_print_expression(out, expr->consequent);
		fprintf(out, ") : (");
		jaf_print_expression(out, expr->alternative);
		fputc(')', out);
		fputc(')', out);
		break;
	case JAF_EXP_FUNCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_BUILTIN_CALL:
	case JAF_EXP_METHOD_CALL:
		jaf_print_expression(out, expr->call.fun);
		print_arglist(out, expr->call.args);
		break;
	case JAF_EXP_SYSCALL:
		fprintf(out, "%s", syscalls[expr->call.func_no].name);
		print_arglist(out, expr->call.args);
		break;
	case JAF_EXP_NEW:
		fprintf(out, "(new ");
		print_type_specifier(out, expr->new.type);
		print_arglist(out, expr->new.args);
		fprintf(out, ")");
		break;
	case JAF_EXP_CAST:
		fprintf(out, "((%s) ", jaf_type_to_string(expr->cast.type));
		jaf_print_expression(out, expr->cast.expr);
		fputc(')', out);
		break;
	case JAF_EXP_MEMBER:
		jaf_print_expression(out, expr->member.struc);
		fprintf(out, ".%s", expr->member.name->text);
		break;
	case JAF_EXP_SEQ:
		fputc('(', out);
		jaf_print_expression(out, expr->seq.head);
		fprintf(out, ", ");
		jaf_print_expression(out, expr->seq.tail);
		fputc(')', out);
		break;
	case JAF_EXP_SUBSCRIPT:
		jaf_print_expression(out, expr->subscript.expr);
		fputc('[', out);
		jaf_print_expression(out, expr->subscript.index);
		fputc(']', out);
		break;
	case JAF_EXP_CHAR:
		// FIXME: shift-jis
		fprintf(out, "'%c'", (char)expr->i);
		break;
	default:
		ERROR("Unhandled expression type: %d", expr->type);
	}
}

static void print_vardecl(FILE *out, struct jaf_vardecl *decl)
{
	print_type_specifier(out, decl->type);
	fprintf(out, " %s", decl->name->text);
	if (decl->array_dims) {
		for (unsigned i = 0; i < decl->type->rank; i++) {
			fputc('[', out);
			jaf_print_expression(out, decl->array_dims[i]);
			fputc(']', out);
		}
	}
	if (decl->init) {
		fprintf(out, " = ");
		jaf_print_expression(out, decl->init);
	}
}

void jaf_print_block_item(FILE *out, struct jaf_block_item *item)
{
	switch (item->kind) {
	case JAF_DECL_VAR:
		print_vardecl(out, &item->var);
		fputc(';', out);
		break;
	case JAF_DECL_FUNCTYPE:
		fprintf(out, "functype ");
		// fallthrough
	case JAF_DECL_FUN:
		print_type_specifier(out, item->fun.type);
		fprintf(out, " %s(", item->fun.name->text);
		if (item->fun.params) {
			struct jaf_block *params = item->fun.params;
			for (size_t i = 0; i < params->nr_items; i++) {
				if (i > 0)
					fprintf(out, ", ");
				assert(params->items[i]->kind == JAF_DECL_VAR);
				print_vardecl(out, &params->items[i]->var);
			}
		}
		fputc(')', out);
		if (item->kind == JAF_DECL_FUN)
			fprintf(out, " { ... }");
		break;
	case JAF_DECL_STRUCT:
		fprintf(out, "struct %s { ... }", item->struc.name->text);
		break;
	case JAF_STMT_LABELED:
	        jaf_print_block_item(out, item->label.stmt);
		break;
	case JAF_STMT_COMPOUND:
		fprintf(out, "{ ... }");
		break;
	case JAF_STMT_EXPRESSION:
		jaf_print_expression(out, item->expr);
		fputc(';', out);
		break;
	case JAF_STMT_IF:
		fprintf(out, "if (");
		jaf_print_expression(out, item->cond.test);
		fprintf(out, ") ...");
		break;
	case JAF_STMT_SWITCH:
		fprintf(out, "switch (");
		jaf_print_expression(out, item->swi.expr);
		fprintf(out, ") ...");
		break;
	case JAF_STMT_WHILE:
		fprintf(out, "while (");
		jaf_print_expression(out, item->while_loop.test);
		fprintf(out, ") ...");
		break;
	case JAF_STMT_DO_WHILE:
		fprintf(out, "do { ... } while (");
		jaf_print_expression(out, item->while_loop.test);
		fprintf(out, ")");
		break;
	case JAF_STMT_FOR:
		fprintf(out, "for (");
		for (size_t i = 0; i < item->for_loop.init->nr_items; i++) {
			jaf_print_block_item(out, item->for_loop.init->items[i]);
		}
		fputc(' ', out);
		jaf_print_expression(out, item->for_loop.test);
		if (item->for_loop.after) {
			fputc(' ', out);
			jaf_print_expression(out, item->for_loop.after);
		}
		fprintf(out, ") ...");
		break;
	case JAF_STMT_GOTO:
		fprintf(out, "goto ???;"); // TODO
		break;
	case JAF_STMT_CONTINUE:
		fprintf(out, "continue;");
		break;
	case JAF_STMT_BREAK:
		fprintf(out, "break;");
		break;
	case JAF_STMT_RETURN:
		fprintf(out, "return");
		if (item->expr) {
			fputc(' ', out);
			jaf_print_expression(out, item->expr);
		}
		fputc(';', out);
		break;
	case JAF_STMT_CASE:
		fprintf(out, "case ");
		jaf_print_expression(out, item->swi_case.expr);
		fprintf(out, ": ...");
		break;
	case JAF_STMT_DEFAULT:
		fprintf(out, "default: ...");
		break;
	case JAF_STMT_MESSAGE:
		fprintf(out, "'%s'", item->msg.text->text);
		if (item->msg.func) {
			fprintf(out, "%s", item->msg.func->text);
		}
		fputc(';', out);
		break;
	case JAF_EOF:
		fprintf(out, "<<EOF>>");
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


noreturn void jaf_generic_error(const char *file, int line, const char *msgf, ...)
{
	va_list ap;
	va_start(ap, msgf);
	jaf_error_msg(file, line, msgf, ap);
	va_end(ap);

	sys_warning("\n");
	sys_exit(1);
}

noreturn void jaf_expression_error(struct jaf_expression *expr, const char *msgf, ...)
{
	va_list ap;
	va_start(ap, msgf);
	jaf_error_msg(expr->file, expr->line, msgf, ap);
	va_end(ap);

	sys_warning(CYAN "\n\tin: " RESET);
	jaf_print_expression(stderr, expr);
	sys_warning("\n");
	sys_exit(1);
}

noreturn void jaf_block_item_error(struct jaf_block_item *item, const char *msgf, ...)
{
	va_list ap;
	va_start(ap, msgf);
	jaf_error_msg(item->file, item->line, msgf, ap);
	va_end(ap);

	sys_warning(CYAN "\n\tin: " RESET);
	jaf_print_block_item(stderr, item);
	sys_warning("\n");
	sys_exit(1);
}
