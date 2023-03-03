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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <zlib.h>
#include "little_endian.h"
#include "system4.h"
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ex.h"
#include "alice/port.h"

static void indent(struct port *port, int level)
{
	for (int i = 0; i < level; i++) {
		port_putc(port, '\t');
	}
}

static void ex_dump_string(struct port *port, struct string *str)
{
	char *u = escape_string(str->text);
	port_printf(port, "\"%s\"", u);
	free(u);
}

static void ex_dump_identifier(struct port *port, struct string *s)
{
	// empty identifier
	if (s->size == 0) {
		port_printf(port, "\"\"");
		return;
	}

	char *u = conv_utf8(s->text);

	// identifier beginning with a digit
	if (s->text[0] >= '0' && s->text[0] <= '9') {
		free(u);
		ex_dump_string(port, s);
		return;
	}

	size_t i = strcspn(u, " \t\r\n\\()[]{}=,.;\"-*");
	if (u[i]) {
		free(u);
		ex_dump_string(port, s);
		return;
	}

	// FIXME: don't reencode if output format is UTF-8 (the default)
	free(u);
	u = conv_output(s->text);
	port_printf(port, "%s", u);
	free(u);
}

static void _ex_dump_table(struct port *port, struct ex_table *table, int indent_level);
static void _ex_dump_list(struct port *port, struct ex_list *list, bool in_line, int indent_level);
static void _ex_dump_tree(struct port *port, struct ex_tree *tree, int indent_level);

static void _ex_dump_value(struct port *port, struct ex_value *val, bool in_line, int indent_level)
{
	switch (val->type) {
	case EX_INT:    port_printf(port, "%d", val->i); break;
	case EX_FLOAT:  port_printf(port, "%f", val->f); break;
	case EX_STRING: ex_dump_string(port, val->s); break;
	case EX_TABLE:  _ex_dump_table(port, val->t, indent_level); break;
	case EX_LIST:   _ex_dump_list(port, val->list, in_line, indent_level); break;
	case EX_TREE:   _ex_dump_tree(port, val->tree, indent_level); break;
	}
}

void ex_dump_value(struct port *port, struct ex_value *val)
{
	_ex_dump_value(port, val, false, 0);
}

void ex_dump_key_value(struct port *port, struct string *key, struct ex_value *val)
{
	port_printf(port, "%s ", ex_strtype(val->type));
	ex_dump_identifier(port, key);
	port_printf(port, " = ");
	ex_dump_value(port, val);
}

static void ex_dump_field(struct port *port, struct ex_field *field, int indent_level)
{
	port_printf(port, "%s%s ", field->is_index ? "indexed " : "", ex_strtype(field->type));
	ex_dump_identifier(port, field->name);
	if (field->has_value) {
		port_printf(port, " = ");
		_ex_dump_value(port, &field->value, true, indent_level);
	}

	if (field->nr_subfields) {
		port_printf(port, " { ");
		for (uint32_t i = 0; i < field->nr_subfields; i++) {
			ex_dump_field(port, &field->subfields[i], indent_level);
			if (i+1 < field->nr_subfields)
				port_printf(port, ", ");
		}
		port_printf(port, " }");
	}
}

static void ex_dump_row(struct port *port, struct ex_value *row, uint32_t nr_columns, int indent_level)
{
	port_printf(port, "{ ");
	for (uint32_t i = 0; i < nr_columns; i++) {
		_ex_dump_value(port, &row[i], true, indent_level);
		if (i+1 < nr_columns)
			port_printf(port, ", ");
	}
	port_printf(port, " }");
}

static void ex_dump_fields(struct port *port, struct ex_table *table, int indent_level)
{
	indent(port, indent_level);
	port_printf(port, "{ ");
	for (uint32_t i = 0; i < table->nr_fields; i++) {
		ex_dump_field(port, &table->fields[i], indent_level);
		if (i+1 < table->nr_fields)
			port_printf(port, ", ");
	}
	port_printf(port, " },\n");
}

void ex_dump_table_row(struct port *port, struct ex_table *table, int row)
{
	port_printf(port, "{\n");
	ex_dump_fields(port, table, 1);
	indent(port, 1);
	ex_dump_row(port, table->rows[row], table->nr_columns, 1);
	port_printf(port, "\n}");
}

static void _ex_dump_table(struct port *port, struct ex_table *table, int indent_level)
{
	bool toplevel = !!table->nr_fields;
	port_putc(port, '{');
	if (toplevel)
		port_putc(port, '\n');

	indent_level++;
	if (table->nr_fields) {
		ex_dump_fields(port, table, indent_level);
	}
	for (uint32_t i = 0; i < table->nr_rows; i++) {
		if (toplevel)
			indent(port, indent_level);
		else
			port_putc(port, ' ');
		ex_dump_row(port, table->rows[i], table->nr_columns, indent_level);
		if (i+1 < table->nr_rows)
			port_putc(port, ',');
		else if (!toplevel)
			port_putc(port, ' ');
		if (toplevel)
			port_putc(port, '\n');
	}
	indent_level--;

	indent(port, indent_level);
	port_putc(port, '}');
}

void ex_dump_table(struct port *port, struct ex_table *table)
{
	_ex_dump_table(port, table, 0);
}

static void _ex_dump_list(struct port *port, struct ex_list *list, bool in_line, int indent_level)
{
	port_putc(port, '{');
	port_putc(port, in_line ? ' ' : '\n');

	indent_level++;
	for (uint32_t i = 0; i < list->nr_items; i++) {
		if (!in_line)
			indent(port, indent_level);
		_ex_dump_value(port, &list->items[i].value, true, indent_level);
		if (i+1 < list->nr_items)
			port_putc(port, ',');
		port_putc(port, in_line ? ' ' : '\n');
	}
	indent_level--;

	port_putc(port, '}');
}

void ex_dump_list(struct port *port, struct ex_list *list)
{
	_ex_dump_list(port, list, false, 0);
}

static void _ex_dump_tree(struct port *port, struct ex_tree *tree, int indent_level)
{
	if (tree->is_leaf) {
		if (tree->leaf.value.type == EX_TABLE)
			port_printf(port, "(table) ");
		else if (tree->leaf.value.type == EX_LIST)
			port_printf(port, "(list) ");
		else if (tree->leaf.value.type == EX_TREE)
			port_printf(port, "(tree) "); // shouldn't happen?
		_ex_dump_value(port, &tree->leaf.value, true, indent_level);
		return;
	}

	port_printf(port, "{\n");

	indent_level++;
	for (uint32_t i = 0; i < tree->nr_children; i++) {
		indent(port, indent_level);
		ex_dump_identifier(port, tree->children[i].name);
		port_printf(port, " = ");
		_ex_dump_tree(port, &tree->children[i], indent_level);
		port_printf(port, ",\n");
	}
	indent_level--;

	indent(port, indent_level);
	port_printf(port, "}");
}

void ex_dump_tree(struct port *port, struct ex_tree *tree)
{
	_ex_dump_tree(port, tree, 0);
}

static void ex_dump_block(struct port *port, struct ex_block *block)
{
	// type name =
	port_printf(port, "%s ", ex_strtype(block->val.type));
	ex_dump_identifier(port, block->name);
	port_printf(port, " = ");

	// rvalue
	switch (block->val.type) {
	case EX_INT:    port_printf(port, "%d", block->val.i); break;
	case EX_FLOAT:  port_printf(port, "%f", block->val.f); break;
	case EX_STRING: ex_dump_string(port, block->val.s); break;
	case EX_TABLE:  ex_dump_table(port, block->val.t); break;
	case EX_LIST:   ex_dump_list(port, block->val.list); break;
	case EX_TREE:   ex_dump_tree(port, block->val.tree); break;
	}

	port_putc(port, ';');
}

void ex_dump(struct port *port, struct ex *ex)
{
	for (uint32_t i = 0; i < ex->nr_blocks; i++) {
		ex_dump_block(port, &ex->blocks[i]);
		if (i+1 < ex->nr_blocks)
			port_printf(port, "\n\n");
	}
	port_putc(port, '\n');
}

void ex_dump_split(FILE *manifest, struct ex *ex, const char *dir)
{
	struct port manifest_port;
	port_file_init(&manifest_port, manifest);

	for (uint32_t i = 0; i < ex->nr_blocks; i++) {
		char buf[PATH_MAX];
		char *name = conv_output(ex->blocks[i].name->text);
		snprintf(buf, PATH_MAX, "%s/%u_%s.x", dir, i, name);

		FILE *out = file_open_utf8(buf, "w");
		if (!out)
			ERROR("Failed to open file '%s': %s", buf, strerror(errno));

		struct port block_port;
		port_file_init(&block_port, out);
		ex_dump_block(&block_port, &ex->blocks[i]);
		port_close(&block_port);

		if (fclose(out))
			ERROR("Failed to close file '%s': %s", buf, strerror(errno));

		fprintf(manifest, "#include \"%u_%s.x\"\n", i, name);
		free(name);
	}

	port_close(&manifest_port);
}
