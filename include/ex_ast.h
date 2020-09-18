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

#ifndef SYSTEM4_EX_AST_H
#define SYSTEM4_EX_AST_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "system4/ex.h"
#include "ex_parser.tab.h"

struct ex *ex_parse(FILE *in);

struct ex_value *ast_make_int(int32_t i);
struct ex_value *ast_make_float(float f);
struct ex_value *ast_make_string(struct string *s);
struct ex_value *ast_make_table_value(struct ex_table *t);
struct ex_value *ast_make_list_value(struct ex_list *list);
struct ex_value *ast_make_tree_value(struct ex_tree *tree);

struct ex *ast_make_ex(block_list *blocks);

struct ex_block *ast_make_int_block(struct string *name, int32_t i);
struct ex_block *ast_make_float_block(struct string *name, float f);
struct ex_block *ast_make_string_block(struct string *name, struct string *s);
struct ex_block *ast_make_table_block(struct string *name, struct ex_table *t);
struct ex_block *ast_make_list_block(struct string *name, struct ex_list *list);
struct ex_block *ast_make_tree_block(struct string *name, struct ex_tree *tree);

block_list *ast_make_block_list(struct ex_block *block);
block_list *ast_block_list_push(block_list *blocks, struct ex_block *block);

struct ex_table *ast_make_table(field_list *fields, row_list *rows);
struct ex_value *ast_make_subtable(row_list *rows);

struct ex_field *ast_make_field_old(int type_token, struct string *name, int uk0, int uk1, int uk2, field_list *subfields);
struct ex_field *ast_make_field(int type_token, struct string *name, struct ex_value *value, bool indexed, field_list *subfields);
field_list *ast_make_field_list(struct ex_field *field);
field_list *ast_field_list_push(field_list *fields, struct ex_field *field);

row_list *ast_make_row_list(value_list *row);
row_list *ast_row_list_push(row_list *rows, value_list *row);

value_list *ast_make_value_list(struct ex_value *value);
value_list *ast_value_list_push(value_list *values, struct ex_value *value);

struct ex_list *ast_make_list(value_list *values);

struct ex_tree *ast_make_tree(node_list *nodes);

node_list *ast_empty_node_list(void);
node_list *ast_make_node_list(struct ex_tree *node);
node_list *ast_node_list_push(node_list *nodes, struct ex_tree *node);

struct ex_tree *ast_make_leaf_int(struct string *name, int32_t i);
struct ex_tree *ast_make_leaf_float(struct string *name, float f);
struct ex_tree *ast_make_leaf_string(struct string *name, struct string *s);
struct ex_tree *ast_make_leaf_table(struct string *name, struct ex_table *t);
struct ex_tree *ast_make_leaf_list(struct string *name, struct ex_list *list);

#endif /* SYSTEM4_EX_AST_H */
