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
#include <errno.h>
#include <assert.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice/jaf.h"

extern unsigned long jaf_line;
extern const char *jaf_file;

struct jaf_expression *jaf_expr(enum jaf_expression_type type, enum jaf_operator op)
{
	struct jaf_expression *e = xcalloc(1, sizeof(struct jaf_expression));
	e->line = jaf_line;
	e->file = jaf_file;
	e->type = type;
	e->op = op;
	return e;
}

struct jaf_expression *jaf_null(void)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_NULL, 0);
	e->valuetype.data = AIN_VOID;
	return e;
}

struct jaf_expression *jaf_integer(int i)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_INT, 0);
	e->valuetype.data = AIN_INT;
	e->i = i;
	return e;
}

struct jaf_expression *jaf_parse_integer(struct string *text)
{
	char *endptr;
	errno = 0;
	int i = strtol(text->text, &endptr, 0);
	if (errno || *endptr != '\0')
		_JAF_ERROR(jaf_file, jaf_line, "Invalid integer constant: %s", text->text);
	free_string(text);
	return jaf_integer(i);
}

struct jaf_expression *jaf_float(float f)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_FLOAT, 0);
	e->valuetype.data = AIN_FLOAT;
	e->f = f;
	return e;
}

struct jaf_expression *jaf_parse_float(struct string *text)
{
	char *endptr;
	errno = 0;
	float f = strtof(text->text, &endptr);
	if (errno || *endptr != '\0')
		_JAF_ERROR(jaf_file, jaf_line, "Invalid floating point constant");
	free_string(text);
	return jaf_float(f);
}

struct string *jaf_process_string(struct string *text)
{
	assert(text->size > 1);
	assert(text->text[0] == '"');

	char *dst = text->text;
	char *src = dst+1;
	while (*src) {
		if (*src == '\\') {
			src++;
			switch (*src++) {
			case 'n':  *dst++ = '\n'; break;
			case 'r':  *dst++ = '\r'; break;
			case 'b':  *dst++ = '\b'; break;
			case '"':  *dst++ = '"';  break;
			case '\\': *dst++ = '\\'; break;
			default: _JAF_ERROR(jaf_file, jaf_line, "Unhandled escape sequence in string");
			}
		} else if (*src == '"') {
			src++;
			while (*src && *src != '"')
				src++;
			if (!*src)
				break;
			src++;
		} else {
			*dst++ = *src++;
		}
	}

	*dst = '\0';
	text->size = dst - text->text;
	return text;
}

struct jaf_expression *jaf_string(struct string *text)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_STRING, 0);
	e->valuetype.data = AIN_STRING;
	e->s = text;
	return e;
}

struct jaf_expression *jaf_char(struct string *text)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_CHAR, 0);
	e->valuetype.data = AIN_INT;
	e->s = text;
	return e;
}

void jaf_name_init(struct jaf_name *name, struct string *str)
{
	memset(name, 0, sizeof(struct jaf_name));
	name->nr_parts = 1;
	name->parts = xmalloc(sizeof(struct string*));
	name->parts[0] = str;
}

void jaf_name_append(struct jaf_name *name, struct string *str)
{
	name->nr_parts++;
	name->parts = xrealloc(name->parts, name->nr_parts * sizeof(struct string*));
	name->parts[name->nr_parts - 1] = str;
}

void jaf_name_prepend(struct jaf_name *name, struct string *str)
{
	name->nr_parts++;
	name->parts = xrealloc(name->parts, name->nr_parts * sizeof(struct string*));
	for (int i = name->nr_parts - 1; i >= 1; i--) {
		name->parts[i] = name->parts[i - 1];
	}
	name->parts[0] = str;
}

struct jaf_expression *jaf_identifier(struct string *name)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_IDENTIFIER, 0);
	e->ident.name = name;
	return e;
}

struct jaf_expression *jaf_this(void)
{
	return jaf_expr(JAF_EXP_THIS, 0);
}

struct string *jaf_method_name(struct string *ns, struct string *name)
{
	string_push_back(&ns, '@');
	string_append(&ns, name);
	free_string(name);
	return ns;
}

struct jaf_expression *jaf_unary_expr(enum jaf_operator op, struct jaf_expression *expr)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_UNARY, op);
	e->expr = expr;
	return e;
}

struct jaf_expression *jaf_binary_expr(enum jaf_operator op, struct jaf_expression *lhs, struct jaf_expression *rhs)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_BINARY, op);
	e->lhs = lhs;
	e->rhs = rhs;
	return e;
}

struct jaf_expression *jaf_ternary_expr(struct jaf_expression *test, struct jaf_expression *cons, struct jaf_expression *alt)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_TERNARY, 0);
	e->condition = test;
	e->consequent = cons;
	e->alternative = alt;
	return e;
}

struct jaf_expression *jaf_seq_expr(struct jaf_expression *head, struct jaf_expression *tail)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_SEQ, 0);
	e->seq.head = head;
	e->seq.tail = tail;
	return e;
}

struct jaf_expression *jaf_function_call(struct jaf_expression *fun, struct jaf_argument_list *args)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_FUNCALL, 0);
	e->call.fun = fun;
	e->call.args = args ? args : xcalloc(1, sizeof(struct jaf_argument_list));
	return e;
}

struct jaf_expression *jaf_new(struct jaf_type_specifier *type, struct jaf_argument_list *args)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_NEW, 0);
	e->new.type = type;
	e->new.type->qualifiers = JAF_QUAL_REF;
	e->new.args = args ? args : xcalloc(1, sizeof(struct jaf_argument_list));
	return e;
}

struct jaf_argument_list *jaf_args(struct jaf_argument_list *head, struct jaf_expression *tail)
{
	if (!head) {
		head = xcalloc(1, sizeof(struct jaf_argument_list));
	}
	head->items = xrealloc_array(head->items, head->nr_items, head->nr_items+1, sizeof(struct jaf_expression*));
	head->items[head->nr_items++] = tail;
	return head;
}

struct jaf_expression *jaf_cast_expression(enum jaf_type type, struct jaf_expression *expr)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_CAST, 0);
	e->cast.type = type;
	e->cast.expr = expr;
	e->valuetype.data = jaf_to_ain_simple_type(type);
	return e;
}

struct jaf_expression *jaf_member_expr(struct jaf_expression *struc, struct string *name)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_MEMBER, 0);
	e->member.struc = struc;
	e->member.name = name;
	return e;
}

struct jaf_expression *jaf_subscript_expr(struct jaf_expression *expr, struct jaf_expression *index)
{
	struct jaf_expression *e = jaf_expr(JAF_EXP_SUBSCRIPT, 0);
	e->subscript.expr = expr;
	e->subscript.index = index;
	return e;
}

struct jaf_type_specifier *jaf_type(enum jaf_type type)
{
	struct jaf_type_specifier *p = xcalloc(1, sizeof(struct jaf_type_specifier));
	if ((enum _jaf_type)type == JAF_INTP) {
		p->type = JAF_INT;
		p->qualifiers = JAF_QUAL_REF;
	} else if ((enum _jaf_type)type == JAF_FLOATP) {
		p->type = JAF_FLOAT;
		p->qualifiers = JAF_QUAL_REF;
	} else {
		p->type = type;
	}
	p->struct_no = -1;
	p->array_type = NULL;
	return p;
}

struct jaf_type_specifier *jaf_typedef(struct string *name)
{
	struct jaf_type_specifier *p = jaf_type(JAF_TYPEDEF);
	p->name = name;
	return p;
}

struct jaf_type_specifier *jaf_array_type(struct jaf_type_specifier *type, int rank)
{
	if (rank < 0)
		_JAF_ERROR(jaf_file, jaf_line, "Negative array rank");
	struct jaf_type_specifier *array_type = jaf_type(JAF_ARRAY);
	array_type->array_type = type;
	array_type->rank = rank;
	return array_type;
}

struct jaf_type_specifier *jaf_wrap(struct jaf_type_specifier *type)
{
	struct jaf_type_specifier *wrap_type = jaf_type(JAF_WRAP);
	wrap_type->array_type = type;
	wrap_type->rank = 1;
	return wrap_type;
}

struct jaf_declarator *jaf_declarator(struct string *name)
{
	struct jaf_declarator *d = xcalloc(1, sizeof(struct jaf_declarator));
	d->name = name;
	return d;
}

struct jaf_declarator *jaf_array_allocation(struct string *name, struct jaf_expression *dim)
{
	struct jaf_declarator *d = xcalloc(1, sizeof(struct jaf_declarator));
	d->name = name;
	d->array_rank = 1;
	d->array_dims = xmalloc(sizeof(struct jaf_expression*));
	d->array_dims[0] = dim;
	return d;
}

struct jaf_declarator *jaf_array_dimension(struct jaf_declarator *d, struct jaf_expression *dim)
{
	int no = d->array_rank;
	d->array_dims = xrealloc_array(d->array_dims, no, no+1, sizeof(struct jaf_expression*));
	d->array_dims[no] = dim;
	d->array_rank++;
	return d;
}

struct jaf_declarator_list *jaf_declarators(struct jaf_declarator_list *head, struct jaf_declarator *tail)
{
	if (!head) {
		head = xcalloc(1, sizeof(struct jaf_declarator_list));
	}

	head->decls = xrealloc_array(head->decls, head->nr_decls, head->nr_decls+1, sizeof(struct jaf_declarator*));
	head->decls[head->nr_decls++] = tail;
	return head;
}

static void init_declaration(struct jaf_type_specifier *type, struct jaf_block_item *dst, struct jaf_declarator *src)
{
	dst->var.type = type;
	if (src) {
		dst->var.name = src->name;
		dst->var.init = src->init;
		dst->var.array_dims = src->array_dims;
		if (src->array_rank && src->array_rank != type->rank)
			JAF_ERROR(dst, "Invalid array declaration");
		free(src);
	} else {
		dst->var.name = make_string("", 0);
	}
}

static struct jaf_block_item *block_item(enum block_item_kind kind)
{
	struct jaf_block_item *item = xcalloc(1, sizeof(struct jaf_block_item));
	item->line = jaf_line;
	item->file = jaf_file;
	item->kind = kind;
	return item;
}

struct jaf_block *jaf_parameter(struct jaf_type_specifier *type, struct jaf_declarator *declarator)
{
	struct jaf_block *p = xmalloc(sizeof(struct jaf_block));
	p->nr_items = 1;
	p->items = xmalloc(sizeof(struct jaf_block_item*));
	p->items[0] = block_item(JAF_DECL_VAR);
	init_declaration(type, p->items[0], declarator);
	return p;
}

struct jaf_function_declarator *jaf_function_declarator(struct jaf_name *name, struct jaf_block *params)
{
	struct jaf_function_declarator *decl = xmalloc(sizeof(struct jaf_function_declarator));
	decl->name = *name;
	decl->params = params;

	// XXX: special case for f(void) functype declarator.
	if (params && params->nr_items == 1 && params->items[0]->var.type->type == JAF_VOID) {
		decl->params = NULL;
		jaf_free_block(params);
	}

	return decl;
}

struct jaf_function_declarator *jaf_function_declarator_simple(struct string *str,
		struct jaf_block *params)
{
	struct jaf_name name;
	jaf_name_init(&name, str);
	return jaf_function_declarator(&name, params);
}

struct jaf_block_item *_jaf_function(struct jaf_type_specifier *type, struct jaf_name *name,
		struct jaf_block *params, struct jaf_block *body)
{
	struct jaf_block_item *item = block_item(JAF_DECL_FUN);
	item->fun.type = type ? type : jaf_type(JAF_VOID);
	item->fun.name = *name;
	item->fun.params = params;
	item->fun.body = body;
	return item;
}

struct jaf_block *jaf_function(struct jaf_type_specifier *type,
		struct jaf_function_declarator *decl, struct jaf_block *body)
{
	struct jaf_block *p = xmalloc(sizeof(struct jaf_block));
	p->nr_items = 1;
	p->items = xmalloc(sizeof(struct jaf_block_item*));
	p->items[0] = _jaf_function(type, &decl->name, decl->params, body);
	free(decl);
	return p;
}

struct jaf_block *jaf_constructor(struct string *name, struct jaf_block *body)
{
	struct jaf_type_specifier *type = jaf_type(JAF_VOID);
	struct jaf_function_declarator *decl = jaf_function_declarator_simple(name, NULL);
	return jaf_function(type, decl, body);
}

struct jaf_block *jaf_destructor(struct string *name, struct jaf_block *body)
{
	struct jaf_type_specifier *type = jaf_type(JAF_VOID);
	struct string *dname = make_string("~", 1);
	string_append(&dname, name);
	free_string(name);
	struct jaf_function_declarator *decl = jaf_function_declarator_simple(dname, NULL);
	return jaf_function(type, decl, body);
}

static struct jaf_type_specifier *copy_type_specifier(struct jaf_type_specifier *type)
{
	if (!type)
		return NULL;
	struct jaf_type_specifier *out = xmalloc(sizeof(struct jaf_type_specifier));
	*out = *type;
	out->array_type = copy_type_specifier(type->array_type);
	out->name = type->name ? string_dup(type->name) : NULL;
	return out;
}

struct jaf_block *jaf_vardecl(struct jaf_type_specifier *type, struct jaf_declarator_list *declarators)
{
	struct jaf_block *decls = xcalloc(1, sizeof(struct jaf_block));
	decls->nr_items = declarators->nr_decls;
	decls->items = xcalloc(declarators->nr_decls, sizeof(struct jaf_block_item*));
	for (size_t i = 0; i < declarators->nr_decls; i++) {
		if (i > 0)
			type = copy_type_specifier(type);
		decls->items[i] = block_item(JAF_DECL_VAR);
		init_declaration(type, decls->items[i], declarators->decls[i]);
	}
	free(declarators->decls);
	free(declarators);
	return decls;
}

struct jaf_block *jaf_merge_blocks(struct jaf_block *head, struct jaf_block *tail)
{
	if (!head)
		return tail;

	size_t nr_decls = head->nr_items + tail->nr_items;
	head->items = xrealloc_array(head->items, head->nr_items, nr_decls, sizeof(struct jaf_toplevel_decl*));

	for (size_t i = 0; i < tail->nr_items; i++) {
		head->items[head->nr_items+i] = tail->items[i];
	}
	head->nr_items = nr_decls;

	free(tail->items);
	free(tail);
	return head;
}

struct jaf_block *jaf_block_append(struct jaf_block *head, struct jaf_block_item *tail)
{
	head->items = xrealloc_array(head->items, head->nr_items, head->nr_items+1, sizeof(struct jaf_block_item*));
	head->items[head->nr_items++] = tail;
	return head;
}

struct jaf_block *jaf_block(struct jaf_block_item *item)
{
	struct jaf_block *block = xcalloc(1, sizeof(struct jaf_block));
	if (!item)
		return block;
	block->items = xmalloc(sizeof(struct jaf_block_item*));
	block->items[0] = item;
	block->nr_items = 1;
	return block;
}

struct jaf_block_item *jaf_compound_statement(struct jaf_block *block)
{
	struct jaf_block_item *item = block_item(JAF_STMT_COMPOUND);
	item->block = block;
	return item;
}

struct jaf_block_item *jaf_label_statement(struct string *label, struct jaf_block_item *stmt)
{
	struct jaf_block_item *item = block_item(JAF_STMT_LABELED);
	item->label.name = label;
	item->label.stmt = stmt;
	return item;
}

struct jaf_block_item *jaf_case_statement(struct jaf_expression *expr, struct jaf_block_item *stmt)
{
	struct jaf_block_item *item = block_item(expr ? JAF_STMT_CASE : JAF_STMT_DEFAULT);
	item->swi_case.expr = expr;
	item->swi_case.stmt = stmt;
	return item;
}

struct jaf_block_item *jaf_message_statement(struct string *msg, struct string *func)
{
	struct jaf_block_item *item = block_item(JAF_STMT_MESSAGE);
	item->msg.text = msg;
	item->msg.func = func;
	return item;
}

struct jaf_block_item *jaf_expression_statement(struct jaf_expression *expr)
{
	// NOTE: character constant as statement is treated as a message
	if (expr->type == JAF_EXP_CHAR) {
		struct string *s = expr->s;
		free(expr);
		return jaf_message_statement(s, NULL);
	}

	struct jaf_block_item *item = block_item(JAF_STMT_EXPRESSION);
	item->expr = expr;
	return item;
}

struct jaf_block_item *jaf_null_statement(void)
{
	struct jaf_block_item *item = block_item(JAF_STMT_NULL);
	return item;
}

struct jaf_block_item *jaf_if_statement(struct jaf_expression *test, struct jaf_block_item *cons, struct jaf_block_item *alt)
{
	struct jaf_block_item *item = block_item(JAF_STMT_IF);
	item->cond.test = test;
	item->cond.consequent = cons;
	item->cond.alternative = alt;
	return item;
}

struct jaf_block_item *jaf_switch_statement(struct jaf_expression *expr, struct jaf_block *body)
{
	struct jaf_block_item *item = block_item(JAF_STMT_SWITCH);
	item->swi.expr = expr;
	item->swi.body = body;
	return item;
}

struct jaf_block_item *jaf_while_loop(struct jaf_expression *test, struct jaf_block_item *body)
{
	struct jaf_block_item *item = block_item(JAF_STMT_WHILE);
	item->while_loop.test = test;
	item->while_loop.body = body;
	return item;
}

struct jaf_block_item *jaf_do_while_loop(struct jaf_expression *test, struct jaf_block_item *body)
{
	struct jaf_block_item *item = block_item(JAF_STMT_DO_WHILE);
	item->while_loop.test = test;
	item->while_loop.body = body;
	return item;
}

struct jaf_block_item *jaf_for_loop(struct jaf_block *init, struct jaf_block_item *test, struct jaf_expression *after, struct jaf_block_item *body)
{
	assert(test->kind == JAF_STMT_EXPRESSION || test->kind == JAF_STMT_NULL);
	struct jaf_block_item *item = block_item(JAF_STMT_FOR);
	item->for_loop.init = init;
	if (test->kind == JAF_STMT_NULL) {
		item->for_loop.test = NULL;
	} else {
		item->for_loop.test = test->expr;
	}
	item->for_loop.after = after;
	item->for_loop.body = body;
	free(test);
	return item;
}

struct jaf_block_item *jaf_goto(struct string *target)
{
	struct jaf_block_item *item = block_item(JAF_STMT_GOTO);
	item->target = target;
	return item;
}

struct jaf_block_item *jaf_continue(void)
{
	return block_item(JAF_STMT_CONTINUE);
}

struct jaf_block_item *jaf_break(void)
{
	return block_item(JAF_STMT_BREAK);
}

struct jaf_block_item *jaf_return(struct jaf_expression *expr)
{
	struct jaf_block_item *item = block_item(JAF_STMT_RETURN);
	item->expr = expr;
	return item;
}

struct jaf_block_item *jaf_struct(struct string *name, struct jaf_block *fields,
		jaf_string_list *interfaces)
{
	struct jaf_block_item *p = block_item(JAF_DECL_STRUCT);
	p->struc.name = name;
	p->struc.members = xcalloc(1, sizeof(struct jaf_block));
	p->struc.methods = xcalloc(1, sizeof(struct jaf_block));
	p->struc.members->items = xcalloc(fields->nr_items, sizeof(struct jaf_block_item*));
	p->struc.methods->items = xcalloc(fields->nr_items, sizeof(struct jaf_block_item*));
	if (interfaces)
		p->struc.interfaces = *interfaces;
	p->struc.struct_no = -1;

	for (unsigned i = 0; i < fields->nr_items; i++) {
		if (fields->items[i]->kind == JAF_DECL_VAR) {
			p->struc.members->items[p->struc.members->nr_items++] = fields->items[i];
		} else if (fields->items[i]->kind == JAF_DECL_FUN) {
			struct jaf_fundecl *decl = &fields->items[i]->fun;
			jaf_name_prepend(&decl->name, string_dup(name));
			p->struc.methods->items[p->struc.methods->nr_items++] = fields->items[i];
		} else {
			_JAF_ERROR(jaf_file, jaf_line, "Unhandled declaration type in struct definition");
		}
	}
	free(fields->items);
	free(fields);
	return p;
}

struct jaf_block_item *jaf_interface(struct string *name, struct jaf_block *methods)
{
	struct jaf_block_item *p = block_item(JAF_DECL_INTERFACE);
	p->struc.name = name;
	p->struc.methods = xcalloc(1, sizeof(struct jaf_block));
	p->struc.methods->items = xcalloc(methods->nr_items, sizeof(struct jaf_block_item*));

	for (unsigned i = 0; i < methods->nr_items; i++) {
		p->struc.methods->items[p->struc.methods->nr_items++] = methods->items[i];
	}
	free(methods->items);
	free(methods);
	return p;
}

struct jaf_block_item *jaf_rassign(struct jaf_expression *lhs, struct jaf_expression *rhs)
{
	struct jaf_block_item *item = block_item(JAF_STMT_RASSIGN);
	item->rassign.lhs = lhs;
	item->rassign.rhs = rhs;
	return item;
}

struct jaf_block_item *jaf_assert(struct jaf_expression *expr, int line, const char *file)
{
	struct string *str = make_string("assert(", 7);
	struct string *expr_str = jaf_expression_to_string(expr);
	string_append(&str, expr_str);
	string_push_back(&str, ')');
	string_push_back(&str, ';');
	free_string(expr_str);

	struct jaf_block_item *item = block_item(JAF_STMT_ASSERT);
	item->assertion.expr = expr;
	item->assertion.expr_string = jaf_string(str);
	item->assertion.line = line;
	item->assertion.file = jaf_string(make_string(file, strlen(file)));
	return item;
}

static struct jaf_argument_list *jaf_copy_argument_list(struct jaf_argument_list *args)
{
	struct jaf_argument_list *out = xmalloc(sizeof(struct jaf_argument_list));
	out->nr_items = args->nr_items;
	out->items = xmalloc(args->nr_items * sizeof(struct jaf_expression*));
	out->var_nos = xmalloc(args->nr_items * sizeof(int));
	for (int i = 0; i < args->nr_items; i++) {
		out->items[i] = jaf_copy_expression(args->items[i]);
	}
	memcpy(out->var_nos, args->var_nos, args->nr_items * sizeof(int));
	return out;
}

static struct jaf_type_specifier *jaf_copy_type_specifier(struct jaf_type_specifier *type)
{
	if (!type)
		return NULL;
	struct jaf_type_specifier *out = xmalloc(sizeof(struct jaf_type_specifier));
	*out = *type;
	if (type->name)
		out->name = string_dup(type->name);
	if (type->array_type)
		out->array_type = jaf_copy_type_specifier(type->array_type);
	return out;
}

struct jaf_expression *jaf_copy_expression(struct jaf_expression *e)
{
	if (!e)
		return NULL;

	struct jaf_expression *out = xmalloc(sizeof(struct jaf_expression));
	*out = *e;

	switch (e->type) {
	case JAF_EXP_VOID:
	case JAF_EXP_INT:
	case JAF_EXP_FLOAT:
	case JAF_EXP_THIS:
	case JAF_EXP_NULL:
		break;
	case JAF_EXP_STRING:
	case JAF_EXP_CHAR:
		out->s = string_dup(e->s);
		break;
	case JAF_EXP_IDENTIFIER:
		out->ident.name = string_dup(e->ident.name);
		if (e->ident.kind == JAF_IDENT_CONST) {
			if (e->ident.constval.data_type == AIN_STRING) {
				out->ident.constval.string_value =
					xstrdup(e->ident.constval.string_value);
			}
		}
		break;
	case JAF_EXP_UNARY:
		out->expr = jaf_copy_expression(e->expr);
		break;
	case JAF_EXP_BINARY:
		out->lhs = jaf_copy_expression(e->lhs);
		out->rhs = jaf_copy_expression(e->rhs);
		break;
	case JAF_EXP_TERNARY:
		out->condition = jaf_copy_expression(e->condition);
		out->consequent = jaf_copy_expression(e->consequent);
		out->alternative = jaf_copy_expression(e->alternative);
		break;
	case JAF_EXP_FUNCALL:
	case JAF_EXP_SYSCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_INTERFACE_CALL:
	case JAF_EXP_BUILTIN_CALL:
	case JAF_EXP_SUPER_CALL:
		out->call.fun = jaf_copy_expression(e->call.fun);
		out->call.args = jaf_copy_argument_list(e->call.args);
		break;
	case JAF_EXP_NEW:
		out->new.type = jaf_copy_type_specifier(e->new.type);
		out->new.args = jaf_copy_argument_list(e->new.args);
		break;
	case JAF_EXP_CAST:
		out->cast.expr = jaf_copy_expression(e->cast.expr);
		break;
	case JAF_EXP_MEMBER:
		out->member.struc = jaf_copy_expression(e->member.struc);
		out->member.name = string_dup(e->member.name);
		break;
	case JAF_EXP_SEQ:
		out->seq.head = jaf_copy_expression(e->seq.head);
		out->seq.tail = jaf_copy_expression(e->seq.tail);
		break;
	case JAF_EXP_SUBSCRIPT:
		out->subscript.expr = jaf_copy_expression(e->subscript.expr);
		out->subscript.index = jaf_copy_expression(e->subscript.index);
		break;
	case JAF_EXP_DUMMYREF:
		out->dummy.expr = jaf_copy_expression(e->dummy.expr);
		break;
	}
	return out;
}

static void jaf_free_name(struct jaf_name name)
{
	for (size_t i = 0; i < name.nr_parts; i++) {
		free_string(name.parts[i]);
	}
	free(name.parts);
	if (name.collapsed) {
		free_string(name.collapsed);
		name.collapsed = NULL;
	}
	name.nr_parts = 0;
	name.parts = NULL;
}

static void jaf_free_argument_list(struct jaf_argument_list *list)
{
	for (size_t i = 0; i < list->nr_items; i++) {
		jaf_free_expr(list->items[i]);
	}
	free(list->items);
	free(list->var_nos);
	free(list);
}

void jaf_free_type_specifier(struct jaf_type_specifier *type)
{
	if (!type)
		return;
	if (type->name)
		free_string(type->name);
	jaf_free_type_specifier(type->array_type);
	free(type);
}

void jaf_free_expr(struct jaf_expression *expr)
{
	if (!expr)
		return;
	switch (expr->type) {
	case JAF_EXP_VOID:
	case JAF_EXP_INT:
	case JAF_EXP_FLOAT:
	case JAF_EXP_THIS:
	case JAF_EXP_NULL:
		break;
	case JAF_EXP_STRING:
	case JAF_EXP_CHAR:
		free_string(expr->s);
		break;
	case JAF_EXP_IDENTIFIER:
		free_string(expr->ident.name);
		if (expr->ident.kind == JAF_IDENT_CONST) {
			if (expr->ident.constval.data_type == AIN_STRING) {
				free(expr->ident.constval.string_value);
			}
		}
		break;
	case JAF_EXP_UNARY:
		jaf_free_expr(expr->expr);
		break;
	case JAF_EXP_BINARY:
		jaf_free_expr(expr->lhs);
		jaf_free_expr(expr->rhs);
		break;
	case JAF_EXP_TERNARY:
		jaf_free_expr(expr->condition);
		jaf_free_expr(expr->consequent);
		jaf_free_expr(expr->alternative);
		break;
	case JAF_EXP_FUNCALL:
	case JAF_EXP_SYSCALL:
	case JAF_EXP_HLLCALL:
	case JAF_EXP_METHOD_CALL:
	case JAF_EXP_INTERFACE_CALL:
	case JAF_EXP_BUILTIN_CALL:
	case JAF_EXP_SUPER_CALL:
		jaf_free_expr(expr->call.fun);
		jaf_free_argument_list(expr->call.args);
		break;
	case JAF_EXP_NEW:
		jaf_free_type_specifier(expr->new.type);
		jaf_free_argument_list(expr->new.args);
		break;
	case JAF_EXP_CAST:
		jaf_free_expr(expr->cast.expr);
		break;
	case JAF_EXP_MEMBER:
		jaf_free_expr(expr->member.struc);
		free_string(expr->member.name);
		break;
	case JAF_EXP_SEQ:
		jaf_free_expr(expr->seq.head);
		jaf_free_expr(expr->seq.tail);
		break;
	case JAF_EXP_SUBSCRIPT:
		jaf_free_expr(expr->subscript.expr);
		jaf_free_expr(expr->subscript.index);
		break;
	case JAF_EXP_DUMMYREF:
		jaf_free_expr(expr->dummy.expr);
		break;
	}
	ain_free_type(&expr->valuetype);
	free(expr);
}

void jaf_free_block_item(struct jaf_block_item *item)
{
	if (!item)
		return;

	switch (item->kind) {
	case JAF_DECL_VAR:
		free_string(item->var.name);
		jaf_free_expr(item->var.init);
		if (item->var.array_dims) {
			for (size_t i = 0; i < item->var.type->rank; i++) {
				jaf_free_expr(item->var.array_dims[i]);
			}
			free(item->var.array_dims);
		}
		jaf_free_type_specifier(item->var.type);
		ain_free_type(&item->var.valuetype);
		break;
	case JAF_DECL_FUNCTYPE:
	case JAF_DECL_DELEGATE:
	case JAF_DECL_FUN:
		jaf_free_name(item->fun.name);
		jaf_free_type_specifier(item->fun.type);
		jaf_free_block(item->fun.params);
		jaf_free_block(item->fun.body);
		ain_free_type(&item->fun.valuetype);
		break;
	case JAF_DECL_STRUCT:
	case JAF_DECL_INTERFACE: {
		free_string(item->struc.name);
		jaf_free_block(item->struc.members);
		jaf_free_block(item->struc.methods);
		struct string *p;
		kv_foreach(p, item->struc.interfaces) {
			free_string(p);
		}
		kv_destroy(item->struc.interfaces);
		break;
	}
	case JAF_STMT_LABELED:
		free_string(item->label.name);
		jaf_free_block_item(item->label.stmt);
		break;
	case JAF_STMT_COMPOUND:
		jaf_free_block(item->block);
		break;
	case JAF_STMT_EXPRESSION:
		jaf_free_expr(item->expr);
		break;
	case JAF_STMT_IF:
		jaf_free_expr(item->cond.test);
		jaf_free_block_item(item->cond.consequent);
		jaf_free_block_item(item->cond.alternative);
		break;
	case JAF_STMT_SWITCH:
		COMPILER_ERROR(item, "switch not supported");
		break;
	case JAF_STMT_WHILE:
	case JAF_STMT_DO_WHILE:
		jaf_free_expr(item->while_loop.test);
		jaf_free_block_item(item->while_loop.body);
		break;
	case JAF_STMT_FOR:
		jaf_free_block(item->for_loop.init);
		jaf_free_expr(item->for_loop.test);
		jaf_free_expr(item->for_loop.after);
		jaf_free_block_item(item->for_loop.body);
		break;
	case JAF_STMT_GOTO:
		free_string(item->target);
	case JAF_STMT_NULL:
	case JAF_STMT_CONTINUE:
	case JAF_STMT_BREAK:
	case JAF_EOF:
		break;
	case JAF_STMT_RETURN:
		jaf_free_expr(item->expr);
		break;
	case JAF_STMT_CASE:
	case JAF_STMT_DEFAULT:
		jaf_free_expr(item->swi_case.expr);
		jaf_free_block_item(item->swi_case.stmt);
		break;
	case JAF_STMT_MESSAGE:
		free_string(item->msg.text);
		if (item->msg.func)
			free_string(item->msg.func);
		break;
	case JAF_STMT_RASSIGN:
		jaf_free_expr(item->rassign.lhs);
		jaf_free_expr(item->rassign.rhs);
		break;
	case JAF_STMT_ASSERT:
		jaf_free_expr(item->assertion.expr);
		jaf_free_expr(item->assertion.expr_string);
		jaf_free_expr(item->assertion.file);
		break;
	}
	kv_destroy(item->delete_vars);
	free(item);
}

void jaf_free_block(struct jaf_block *block)
{
	if (!block)
		return;
	for (size_t i = 0; i < block->nr_items; i++) {
		jaf_free_block_item(block->items[i]);
	}
	free(block->items);
	free(block);
}
