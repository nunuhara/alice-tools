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

#include <stdio.h>
#include <string.h>
#include "system4.h"
#include "system4/ex.h"
#include "system4/string.h"
#include "ex_parser.tab.h"
#include "ast.h"

extern FILE *yex_in;

#define flatten_list(type, list, outvar)				\
	do {								\
		outvar = xcalloc(kv_size(*list), sizeof(type));		\
		for (size_t _fl_i = 0; _fl_i < kv_size(*list); _fl_i++) { \
			outvar[_fl_i] = *kv_A(*list, _fl_i);		\
			free(kv_A(*list, _fl_i));			\
		}							\
		kv_destroy(*list);					\
		free(list);						\
	} while (0)

struct ex *ex_parse(FILE *in)
{
	yex_in = in;
	yex_parse();
	return ex_data;
}

/*
 * Values
 */

enum ex_value_type ast_token_to_value_type(int token)
{
	switch (token) {
	case INT:    return EX_INT;
	case FLOAT:  return EX_FLOAT;
	case STRING: return EX_STRING;
	case TABLE:  return EX_TABLE;
	case LIST:   return EX_LIST;
	case TREE:   return EX_TREE;
	default:     ERROR("Invalid token type: %d", token);
	}
}

static struct ex_value *ast_alloc_value(enum ex_value_type type)
{
	struct ex_value *v = xmalloc(sizeof(struct ex_value));
	v->type = type;
	return v;
}

#define ast_return_value(extype, init)			\
	struct ex_value *var = ast_alloc_value(extype); \
	var->init;					\
	return var;

struct ex_value *ast_make_int(int32_t i)
{
	ast_return_value(EX_INT, i = i);
}

struct ex_value *ast_make_float(float f)
{
	ast_return_value(EX_FLOAT, f = f);
}

struct ex_value *ast_make_string(struct string *s)
{
	ast_return_value(EX_STRING, s = s);
}

struct ex_value *ast_make_table_value(struct ex_table *t)
{
	ast_return_value(EX_TABLE, t = t);
}

struct ex_value *ast_make_list_value(struct ex_list *list)
{
	ast_return_value(EX_LIST, list = list);
}

struct ex_value *ast_make_tree_value(struct ex_tree *tree)
{
	ast_return_value(EX_TREE, tree = tree);
}

/*
 * Ex file
 */

struct ex *ast_make_ex(block_list *blocks)
{
	struct ex *ex = xmalloc(sizeof(struct ex));
	ex->nr_blocks = kv_size(*blocks);
	flatten_list(struct ex_block, blocks, ex->blocks);
	return ex;
}

/*
 * Blocks
 */

#define ast_return_block(name, extype, init)			 \
	struct ex_block *var = xmalloc(sizeof(struct ex_block)); \
	var->name = name;					 \
	var->val.type = extype;					 \
	var->val.init;						 \
	return var;

struct ex_block *ast_make_int_block(struct string *name, int32_t i)
{
	ast_return_block(name, EX_INT, i = i);
}

struct ex_block *ast_make_float_block(struct string *name, float f)
{
	ast_return_block(name, EX_FLOAT, f = f);
}

struct ex_block *ast_make_string_block(struct string *name, struct string *s)
{
	ast_return_block(name, EX_STRING, s = s);
}

struct ex_block *ast_make_table_block(struct string *name, struct ex_table *t)
{
	ast_return_block(name, EX_TABLE, t = t);
}

struct ex_block *ast_make_list_block(struct string *name, struct ex_list *list)
{
	ast_return_block(name, EX_LIST, list = list);
}

struct ex_block *ast_make_tree_block(struct string *name, struct ex_tree *tree)
{
	tree->name = string_ref(name);
	ast_return_block(name, EX_TREE, tree = tree);
}

block_list *ast_make_block_list(struct ex_block *block)
{
	block_list *list = xmalloc(sizeof(block_list));
	kv_init(*list);
	return ast_block_list_push(list, block);
}

block_list *ast_block_list_push(block_list *blocks, struct ex_block *block)
{
	kv_push(struct ex_block*, *blocks, block);
	return blocks;
}

/*
 * Tables
 */

struct ex_table *ast_make_table(field_list *fields, row_list *rows)
{
	struct ex_table *table = xmalloc(sizeof(struct ex_table));
	table->nr_fields = kv_size(*fields);
	table->nr_columns = table->nr_fields;
	flatten_list(struct ex_field, fields, table->fields);

	if (!rows) {
		table->nr_rows = 0;
		table->rows = NULL;
		return table;
	}

	table->nr_rows = kv_size(*rows);

	table->rows = xcalloc(table->nr_rows, sizeof(struct ex_value*));
	for (size_t i = 0; i < table->nr_rows; i++) {
		value_list *cells = kv_A(*rows, i);
		if (kv_size(*cells) != table->nr_columns)
			ERROR("Row has wrong number of columns");
		flatten_list(struct ex_value, cells, table->rows[i]);
	}

	kv_destroy(*rows);
	free(rows);
	return table;
}

struct ex_value *ast_make_subtable(row_list *rows)
{
	struct ex_table *table = xmalloc(sizeof(struct ex_table));
	table->nr_fields = 0;
	table->fields = NULL;
	table->nr_columns = 0;

	if (!rows) {
		table->nr_rows = 0;
		table->rows = NULL;
		return ast_make_table_value(table);
	}

	table->nr_columns = kv_size(*kv_A(*rows, 0));
	table->nr_rows = kv_size(*rows);
	table->rows = xcalloc(table->nr_rows, sizeof(struct ex_value*));
	for (size_t i = 0; i < table->nr_rows; i++) {
		value_list *cells = kv_A(*rows, i);
		if (kv_size(*cells) != table->nr_columns)
			ERROR("Number of columns in sub-table is not constant");
		flatten_list(struct ex_value, cells, table->rows[i]);
	}

	kv_destroy(*rows);
	free(rows);
	return ast_make_table_value(table);
}

/*
 * Fields
 */

// XXX: for deprecated syntax; `value` needs to be a full value type
struct ex_field *ast_make_field_old(int type, struct string *name, int has_value, int indexed, int value, field_list *subfields)
{
	struct ex_field *field = xmalloc(sizeof(struct ex_field));
	*field = (struct ex_field) {
		.type = type,
		.name = name,
		.has_value = has_value,
		.value = {
			.type = type,
			.i = value
		},
		.is_index = indexed,
		.nr_subfields = 0,
		.subfields = NULL
	};
	if (has_value && type == EX_STRING) {
		field->value.s = make_string("", 0);
	}
	if (subfields) {
		field->nr_subfields = kv_size(*subfields);
		flatten_list(struct ex_field, subfields, field->subfields);
	}
	return field;
}

struct ex_field *ast_make_field(int type, struct string *name, struct ex_value *value, bool indexed, field_list *subfields)
{
	struct ex_field *field = xmalloc(sizeof(struct ex_field));
	*field = (struct ex_field) {
		.type = type,
		.name = name,
		.has_value = !!value,
		.is_index = indexed,
		.nr_subfields = 0,
		.subfields = NULL
	};
	if (value) {
		if ((int)value->type != type) {
			WARNING("Field value type doesn't match field type");
		}
		field->value = *value;
	}
	if (subfields) {
		field->nr_subfields = kv_size(*subfields);
		flatten_list(struct ex_field, subfields, field->subfields);
	}
	if (indexed && value && (int)value->type != EX_INT && (int)value->type != EX_STRING) {
		WARNING("Unexpected type for indexed field: %d", value->type);
	}
	free(value);
	return field;
}

field_list *ast_make_field_list(struct ex_field *field)
{
	field_list *list = xmalloc(sizeof(field_list));
	kv_init(*list);
	return ast_field_list_push(list, field);
}

field_list *ast_field_list_push(field_list *fields, struct ex_field *field)
{
	kv_push(struct ex_field*, *fields, field);
	return fields;
}

/*
 * Rows
 */

row_list *ast_make_row_list(value_list *row)
{
	row_list *list = xmalloc(sizeof(row_list));
	kv_init(*list);
	return ast_row_list_push(list, row);
}

row_list *ast_row_list_push(row_list *rows, value_list *row)
{
	kv_push(value_list*, *rows, row);
	return rows;
}

/*
 * Cells
 */

value_list *ast_make_value_list(struct ex_value *value)
{
	value_list *list = xmalloc(sizeof(value_list));
	kv_init(*list);
	return ast_value_list_push(list, value);
}

value_list *ast_value_list_push(value_list *values, struct ex_value *value)
{
	kv_push(struct ex_value*, *values, value);
	return values;
}

/*
 * Lists
 */

struct ex_list *ast_make_list(value_list *values)
{
	struct ex_list *list = xmalloc(sizeof(struct ex_list));
	if (!values) {
		list->nr_items = 0;
		list->items = NULL;
		return list;
	}
	list->nr_items = kv_size(*values);
	list->items = xcalloc(list->nr_items, sizeof(struct ex_list_item));
	for (size_t i = 0; i < list->nr_items; i++) {
		struct ex_value *v = kv_A(*values, i);
		list->items[i].value = *v;
		free(v);
	}
	kv_destroy(*values);
	free(values);
	return list;
}

/*
 * Trees
 */

struct ex_tree *ast_make_tree(node_list *nodes)
{
	struct ex_tree *tree = xmalloc(sizeof(struct ex_tree));
	tree->name = NULL;
	tree->is_leaf = false;
	if (!nodes) {
		tree->nr_children = 0;
		tree->children = NULL;
		return tree;
	}
	tree->nr_children = kv_size(*nodes);
	flatten_list(struct ex_tree, nodes, tree->children);
	return tree;
}

node_list *ast_empty_node_list(void)
{
	node_list *list = xmalloc(sizeof(node_list));
	kv_init(*list);
	return list;
}

node_list *ast_make_node_list(struct ex_tree *node)
{
	return ast_node_list_push(ast_empty_node_list(), node);
}

node_list *ast_node_list_push(node_list *nodes, struct ex_tree *node)
{
	kv_push(struct ex_tree*, *nodes, node);
	return nodes;
}

struct ex_tree *ast_make_leaf_int(struct string *name, int32_t i)
{
	struct ex_tree *tree = xmalloc(sizeof(struct ex_tree));
	*tree = (struct ex_tree) {
		.name = name,
		.is_leaf = true,
		.leaf = {
			.name = string_ref(name),
			.value = {
				.type = EX_INT,
				.i = i
			}
		}
	};
	return tree;
}

struct ex_tree *ast_make_leaf_float(struct string *name, float f)
{
	struct ex_tree *tree = xmalloc(sizeof(struct ex_tree));
	*tree = (struct ex_tree) {
		.name = name,
		.is_leaf = true,
		.leaf = {
			.name = string_ref(name),
			.value = {
				.type = EX_FLOAT,
				.f = f
			}
		}
	};
	return tree;
}

struct ex_tree *ast_make_leaf_string(struct string *name, struct string *s)
{
	struct ex_tree *tree = xmalloc(sizeof(struct ex_tree));
	*tree = (struct ex_tree) {
		.name = name,
		.is_leaf = true,
		.leaf = {
			.name = string_ref(name),
			.value = {
				.type = EX_STRING,
				.s = s
			}
		}
	};
	return tree;
}

struct ex_tree *ast_make_leaf_table(struct string *name, struct ex_table *t)
{
	struct ex_tree *tree = xmalloc(sizeof(struct ex_tree));
	*tree = (struct ex_tree) {
		.name = name,
		.is_leaf = true,
		.leaf = {
			.name = string_ref(name),
			.value = {
				.type = EX_TABLE,
				.t = t
			}
		}
	};
	return tree;
}

struct ex_tree *ast_make_leaf_list(struct string *name, struct ex_list *list)
{
	struct ex_tree *tree = xmalloc(sizeof(struct ex_tree));
	*tree = (struct ex_tree) {
		.name = name,
		.is_leaf = true,
		.leaf = {
			.name = string_ref(name),
			.value = {
				.type = EX_LIST,
				.list = list
			}
		}
	};
	return tree;
}

