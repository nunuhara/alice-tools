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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include "system4.h"
#include "system4/ex.h"
#include "system4/string.h"

static bool ex_compare(struct ex *a, struct ex *b);

static void usage(void)
{
	puts("Usage: excmp <exfile> <exfile>");
	puts("    Compare two ex files");
}

enum {
	LOPT_HELP = 256,
};

int main(int argc, char *argv[])
{
	while (1) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, LOPT_HELP },
		};
		int option_index = 0;
		int c = getopt_long(argc, argv, "h", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case LOPT_HELP:
			usage();
			return 0;
		case '?':
			ERROR("Unexpected command line argument");
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2) {
		usage();
		ERROR("Wrong number of arguments");
		return 1;
	}

	struct ex *a, *b;
	if (!(a = ex_read(argv[0])) || !(b = ex_read(argv[1]))) {
		ERROR("Failed to open ex file");
	}

	ex_compare(a, b);
	ex_free(a);
	ex_free(b);
	return 0;
}

static bool ex_value_equal(struct ex_value *a, struct ex_value *b);

static bool ex_field_equal(struct ex_field *a, struct ex_field *b)
{
	if (a->type != b->type)
		return false;
	if (strcmp(a->name->text, b->name->text))
		return false;
	if (a->has_value != b->has_value)
		return false;
	if (a->is_index != b->is_index)
		return false;
	if (a->has_value && !ex_value_equal(&a->value, &b->value))
		return false;
	if (a->nr_subfields != b->nr_subfields)
		return false;
	for (unsigned i = 0; i < a->nr_subfields; i++) {
		if (!ex_field_equal(&a->subfields[i], &b->subfields[i]))
			return false;
	}
	return true;
}

static bool ex_table_equal(struct ex_table *a, struct ex_table *b)
{
	if (a->nr_fields != b->nr_fields)
		return false;
	for (unsigned i = 0; i < a->nr_fields; i++) {
		if (!ex_field_equal(&a->fields[i], &b->fields[i]))
			return false;
	}
	if (a->nr_columns != b->nr_columns)
		return false;
	if (a->nr_rows != b->nr_rows)
		return false;
	for (unsigned i = 0; i < a->nr_rows; i++) {
		for (unsigned j = 0; j < a->nr_columns; j++) {
			if (!ex_value_equal(&a->rows[i][j], &b->rows[i][j]))
				return false;
		}
	}
	return true;
}

static bool ex_list_equal(struct ex_list *a, struct ex_list *b)
{
	if (a->nr_items != b->nr_items)
		return false;
	for (unsigned i = 0; i < a->nr_items; i++) {
		if (a->items[i].size != b->items[i].size)
			return false;
		if (!ex_value_equal(&a->items[i].value, &b->items[i].value))
			return false;
	}
	return true;
}

static bool ex_tree_equal(struct ex_tree *a, struct ex_tree *b)
{
	if (strcmp(a->name->text, b->name->text))
		return false;
	if (a->is_leaf != b->is_leaf)
		return false;
	if (a->is_leaf) {
		if (a->leaf.size != b->leaf.size)
			return false;
		if (strcmp(a->leaf.name->text, a->leaf.name->text))
			return false;
		if (!ex_value_equal(&a->leaf.value, &b->leaf.value))
			return false;
	} else {
		if (a->nr_children != b->nr_children)
			return false;
		for (unsigned i = 0; i < a->nr_children; i++) {
			if (!ex_tree_equal(&a->children[i], &b->children[i]))
				return false;
		}
	}
	return true;
}

static bool ex_value_equal(struct ex_value *a, struct ex_value *b)
{
	if (a->type != b->type)
		return false;
	switch (a->type) {
	case EX_INT:
		return a->i == b->i;
	case EX_FLOAT:
		return fabsf(a->f - b->f) < 0.0001;
	case EX_STRING:
		return !strcmp(a->s->text, b->s->text);
	case EX_TABLE:
		return ex_table_equal(a->t, b->t);
	case EX_LIST:
		return ex_list_equal(a->list, b->list);
	case EX_TREE:
		return ex_tree_equal(a->tree, b->tree);
	default:
		ERROR("Unrecognized type: %d", a->type);
	}
}

static bool ex_compare(struct ex *a, struct ex *b)
{
	if (a->nr_blocks != b->nr_blocks) {
		printf("number of blocks differs (%u vs %u)", a->nr_blocks, b->nr_blocks);
		return false;
	}
	for (unsigned i = 0; i < a->nr_blocks; i++) {
		if (strcmp(a->blocks[i].name->text, b->blocks[i].name->text)) {
			printf("Block name differs for block %u (\"%s\" vs \"%s\")", i,
			       a->blocks[i].name->text, b->blocks[i].name->text);
			return false;
		}
		if (a->blocks[i].size != b->blocks[i].size) {
			printf("Block size differs for \"%s\" (%u vs %u)", a->blocks[i].name->text,
			       (unsigned)a->blocks[i].size, (unsigned)b->blocks[i].size);
			return false;
		}
		if (!ex_value_equal(&a->blocks[i].val, &b->blocks[i].val)) {
			printf("Block value differs for \"%s\" (block %u)", a->blocks[i].name->text, i);
			return false;
		}
	}
	return true;
}
