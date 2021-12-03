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

int indent_level = 0;

static void indent(struct port *port)
{
	for (int i = 0; i < indent_level; i++) {
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

static void ex_dump_table(struct port *port, struct ex_table *table);
static void ex_dump_list(struct port *port, struct ex_list *list, bool in_line);

static void ex_dump_value(struct port *port, struct ex_value *val)
{
	switch (val->type) {
	case EX_INT:    port_printf(port, "%d", val->i); break;
	case EX_FLOAT:  port_printf(port, "%f", val->f); break;
	case EX_STRING: ex_dump_string(port, val->s); break;
	case EX_TABLE:  ex_dump_table(port, val->t); break;
	case EX_LIST:   ex_dump_list(port, val->list, true); break;
	case EX_TREE:   ERROR("TODO DUMP TREE"); break;
	}
}

static void ex_dump_field(struct port *port, struct ex_field *field)
{
	port_printf(port, "%s%s ", field->is_index ? "indexed " : "", ex_strtype(field->type));
	ex_dump_identifier(port, field->name);
	if (field->has_value) {
		port_printf(port, " = ");
		ex_dump_value(port, &field->value);
	}

	if (field->nr_subfields) {
		port_printf(port, " { ");
		for (uint32_t i = 0; i < field->nr_subfields; i++) {
			ex_dump_field(port, &field->subfields[i]);
			if (i+1 < field->nr_subfields)
				port_printf(port, ", ");
		}
		port_printf(port, " }");
	}
}

static void ex_dump_row(struct port *port, struct ex_value *row, uint32_t nr_columns)
{
	port_printf(port, "{ ");
	for (uint32_t i = 0; i < nr_columns; i++) {
		ex_dump_value(port, &row[i]);
		if (i+1 < nr_columns)
			port_printf(port, ", ");
	}
	port_printf(port, " }");
}

static void ex_dump_table(struct port *port, struct ex_table *table)
{
	bool toplevel = !!table->nr_fields;
	port_putc(port, '{');
	if (toplevel)
		port_putc(port, '\n');

	indent_level++;
	if (table->nr_fields) {
		indent(port);
		port_printf(port, "{ ");
		for (uint32_t i = 0; i < table->nr_fields; i++) {
			ex_dump_field(port, &table->fields[i]);
			if (i+1 < table->nr_fields)
				port_printf(port, ", ");
		}
		port_printf(port, " },\n");
	}
	for (uint32_t i = 0; i < table->nr_rows; i++) {
		if (toplevel)
			indent(port);
		else
			port_putc(port, ' ');
		ex_dump_row(port, table->rows[i], table->nr_columns);
		if (i+1 < table->nr_rows)
			port_putc(port, ',');
		else if (!toplevel)
			port_putc(port, ' ');
		if (toplevel)
			port_putc(port, '\n');
	}
	indent_level--;

	indent(port);
	port_putc(port, '}');
}

static void ex_dump_list(struct port *port, struct ex_list *list, bool in_line)
{
	port_putc(port, '{');
	port_putc(port, in_line ? ' ' : '\n');

	indent_level++;
	for (uint32_t i = 0; i < list->nr_items; i++) {
		if (!in_line)
			indent(port);
		ex_dump_value(port, &list->items[i].value);
		if (i+1 < list->nr_items)
			port_putc(port, ',');
		port_putc(port, in_line ? ' ' : '\n');
	}
	indent_level--;

	port_putc(port, '}');
}

static void ex_dump_tree(struct port *port, struct ex_tree *tree, int level)
{
	if (tree->is_leaf) {
		if (tree->leaf.value.type == EX_TABLE)
			port_printf(port, "(table) ");
		else if (tree->leaf.value.type == EX_LIST)
			port_printf(port, "(list) ");
		else if (tree->leaf.value.type == EX_TREE)
			port_printf(port, "(tree) "); // shouldn't happen?
		ex_dump_value(port, &tree->leaf.value);
		return;
	}

	port_printf(port, "{\n");

	indent_level++;
	for (uint32_t i = 0; i < tree->nr_children; i++) {
		indent(port);
		ex_dump_identifier(port, tree->children[i].name);
		port_printf(port, " = ");
		ex_dump_tree(port, &tree->children[i], level+1);
		port_printf(port, ",\n");
	}
	indent_level--;

	indent(port);
	port_printf(port, "}");
}

static void ex_dump_block(struct port *port, struct ex_block *block)
{
	indent_level = 0;
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
	case EX_LIST:   ex_dump_list(port, block->val.list, false); break;
	case EX_TREE:   ex_dump_tree(port, block->val.tree, 0); break;
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
	for (uint32_t i = 0; i < ex->nr_blocks; i++) {
		char buf[PATH_MAX];
		char *name = conv_output(ex->blocks[i].name->text);
		snprintf(buf, PATH_MAX, "%s/%u_%s.x", dir, i, name);

		FILE *out = file_open_utf8(buf, "w");
		if (!out)
			ERROR("Failed to open file '%s': %s", buf, strerror(errno));

		struct port port;
		port_file_init(&port, manifest);
		ex_dump_block(&port, &ex->blocks[i]);
		port_close(&port);

		if (fclose(out))
			ERROR("Failed to close file '%s': %s", buf, strerror(errno));

		fprintf(manifest, "#include \"%u_%s.x\"\n", i, name);
		free(name);
	}
}
