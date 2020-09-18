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

int indent_level = 0;

static void indent(FILE *out)
{
	for (int i = 0; i < indent_level; i++) {
		fputc('\t', out);
	}
}

static void ex_dump_string(FILE *out, struct string *str)
{
	char *u = escape_string(str->text);
	fprintf(out, "\"%s\"", u);
	free(u);
}

static void ex_dump_identifier(FILE *out, struct string *s)
{
	// empty identifier
	if (s->size == 0) {
		fprintf(out, "\"\"");
		return;
	}

	char *u = conv_utf8(s->text);

	// identifier beginning with a digit
	if (s->text[0] >= '0' && s->text[0] <= '9') {
		free(u);
		ex_dump_string(out, s);
		return;
	}

	size_t i = strcspn(u, " \t\r\n\\()[]{}=,.;\"-");
	if (u[i]) {
		free(u);
		ex_dump_string(out, s);
		return;
	}

	// FIXME: don't reencode if output format is UTF-8 (the default)
	free(u);
	u = conv_output(s->text);
	fprintf(out, "%s", u);
	free(u);
}

static void ex_dump_table(FILE *out, struct ex_table *table);
static void ex_dump_list(FILE *out, struct ex_list *list, bool in_line);

static void ex_dump_value(FILE *out, struct ex_value *val)
{
	switch (val->type) {
	case EX_INT:    fprintf(out, "%d", val->i); break;
	case EX_FLOAT:  fprintf(out, "%f", val->f); break;
	case EX_STRING: ex_dump_string(out, val->s); break;
	case EX_TABLE:  ex_dump_table(out, val->t); break;
	case EX_LIST:   ex_dump_list(out, val->list, true); break;
	case EX_TREE:   ERROR("TODO DUMP TREE"); break;
	}
}

static void ex_dump_field(FILE *out, struct ex_field *field)
{
	fprintf(out, "%s%s ", field->is_index ? "indexed " : "", ex_strtype(field->type));
	ex_dump_identifier(out, field->name);
	if (field->has_value) {
		fprintf(out, " = ");
		ex_dump_value(out, &field->value);
	}

	if (field->nr_subfields) {
		fprintf(out, " { ");
		for (uint32_t i = 0; i < field->nr_subfields; i++) {
			ex_dump_field(out, &field->subfields[i]);
			if (i+1 < field->nr_subfields)
				fprintf(out, ", ");
		}
		fprintf(out, " }");
	}
}

static void ex_dump_row(FILE *out, struct ex_value *row, uint32_t nr_columns)
{
	fprintf(out, "{ ");
	for (uint32_t i = 0; i < nr_columns; i++) {
		ex_dump_value(out, &row[i]);
		if (i+1 < nr_columns)
			fprintf(out, ", ");
	}
	fprintf(out, " }");
}

static void ex_dump_table(FILE *out, struct ex_table *table)
{
	bool toplevel = !!table->nr_fields;
	fputc('{', out);
	if (toplevel)
		fputc('\n', out);

	indent_level++;
	if (table->nr_fields) {
		indent(out);
		fprintf(out, "{ ");
		for (uint32_t i = 0; i < table->nr_fields; i++) {
			ex_dump_field(out, &table->fields[i]);
			if (i+1 < table->nr_fields)
				fprintf(out, ", ");
		}
		fprintf(out, " },\n");
	}
	for (uint32_t i = 0; i < table->nr_rows; i++) {
		if (toplevel)
			indent(out);
		else
			fputc(' ', out);
		ex_dump_row(out, table->rows[i], table->nr_columns);
		if (i+1 < table->nr_rows)
			fputc(',', out);
		else if (!toplevel)
			fputc(' ', out);
		if (toplevel)
			fputc('\n', out);
	}
	indent_level--;

	indent(out);
	fputc('}', out);
}

static void ex_dump_list(FILE *out, struct ex_list *list, bool in_line)
{
	fputc('{', out);
	fputc(in_line ? ' ' : '\n', out);

	indent_level++;
	for (uint32_t i = 0; i < list->nr_items; i++) {
		if (!in_line)
			indent(out);
		ex_dump_value(out, &list->items[i].value);
		if (i+1 < list->nr_items)
			fputc(',', out);
		fputc(in_line ? ' ' : '\n', out);
	}
	indent_level--;

	fputc('}', out);
}

static void ex_dump_tree(FILE *out, struct ex_tree *tree, int level)
{
	if (tree->is_leaf) {
		if (tree->leaf.value.type == EX_TABLE)
			fprintf(out, "(table) ");
		else if (tree->leaf.value.type == EX_LIST)
			fprintf(out, "(list) ");
		else if (tree->leaf.value.type == EX_TREE)
			fprintf(out, "(tree) "); // shouldn't happen?
		ex_dump_value(out, &tree->leaf.value);
		return;
	}

	fprintf(out, "{\n");

	indent_level++;
	for (uint32_t i = 0; i < tree->nr_children; i++) {
		indent(out);
		ex_dump_identifier(out, tree->children[i].name);
		fprintf(out, " = ");
		ex_dump_tree(out, &tree->children[i], level+1);
		fprintf(out, ",\n");
	}
	indent_level--;

	indent(out);
	fprintf(out, "}");
}

static void ex_dump_block(FILE *out, struct ex_block *block)
{
	indent_level = 0;
	// type name =
	fprintf(out, "%s ", ex_strtype(block->val.type));
	ex_dump_identifier(out, block->name);
	fprintf(out, " = ");

	// rvalue
	switch (block->val.type) {
	case EX_INT:    fprintf(out, "%d", block->val.i); break;
	case EX_FLOAT:  fprintf(out, "%f", block->val.f); break;
	case EX_STRING: ex_dump_string(out, block->val.s); break;
	case EX_TABLE:  ex_dump_table(out, block->val.t); break;
	case EX_LIST:   ex_dump_list(out, block->val.list, false); break;
	case EX_TREE:   ex_dump_tree(out, block->val.tree, 0); break;
	}

	fputc(';', out);
}

void ex_dump(FILE *out, struct ex *ex)
{
	for (uint32_t i = 0; i < ex->nr_blocks; i++) {
		ex_dump_block(out, &ex->blocks[i]);
		if (i+1 < ex->nr_blocks)
			fprintf(out, "\n\n");
	}
	fputc('\n', out);
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

		ex_dump_block(out, &ex->blocks[i]);

		if (fclose(out))
			ERROR("Failed to close file '%s': %s", buf, strerror(errno));

		fprintf(manifest, "#include \"%u_%s.x\"\n", i, name);
		free(name);
	}
}
