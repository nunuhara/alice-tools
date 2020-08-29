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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>
#include "system4.h"
#include "system4/buffer.h"
#include "system4/ex.h"
#include "system4/string.h"

bool columns_first = false;

static void write_table(struct buffer *out, struct ex_table *table);
static void write_list(struct buffer *out, struct ex_list *list);
static void write_tree(struct buffer *out, struct ex_tree *tree);

static size_t skip_int32(struct buffer *out)
{
	size_t loc = out->index;
	buffer_skip(out, 4);
	return loc;
}

static void write_string(struct buffer *out, struct string *s)
{
	size_t padded_size = (s->size + 3) & ~3;
	buffer_write_int32(out, padded_size);
	buffer_write_bytes(out, (uint8_t*)s->text, s->size);

	static const uint8_t pad[4] = { 0, 0, 0, 0 };
	if (padded_size - s->size)
		buffer_write_bytes(out, pad, padded_size - s->size);
}

static void _write_value(struct buffer *out, struct ex_value *v)
{
	switch (v->type) {
	case EX_INT:
		buffer_write_int32(out, v->i);
		break;
	case EX_FLOAT:
		buffer_write_float(out, v->f);
		break;
	case EX_STRING:
		write_string(out, v->s);
		break;
	case EX_TABLE:
		write_table(out, v->t);
		break;
	case EX_LIST:
		write_list(out, v->list);
		break;
	case EX_TREE:
		write_tree(out, v->tree);
		break;
	}
}

static void write_value(struct buffer *out, struct ex_value *v)
{
	buffer_write_int32(out, v->type);
	_write_value(out, v);
}

static void write_field(struct buffer *out, struct ex_field *f)
{
	buffer_write_int32(out, f->type);
	write_string(out, f->name);
	buffer_write_int32(out, f->has_value);
	buffer_write_int32(out, f->is_index);
	if (f->has_value) {
		_write_value(out, &f->value);
	}

	if (f->type == EX_TABLE) {
		buffer_write_int32(out, f->nr_subfields);
		for (uint32_t i = 0; i < f->nr_subfields; i++) {
			write_field(out, &f->subfields[i]);
		}
	}
}

static void write_rows(struct buffer *out, struct ex_table *table)
{
	if (columns_first) {
		buffer_write_int32(out, table->nr_columns);
		buffer_write_int32(out, table->nr_rows);
	} else {
		buffer_write_int32(out, table->nr_rows);
		buffer_write_int32(out, table->nr_columns);
	}

	for (uint32_t i = 0; i < table->nr_rows; i++) {
		for (uint32_t j = 0; j < table->nr_columns; j++) {
			write_value(out, &table->rows[i][j]);
		}
	}
}

static void write_table(struct buffer *out, struct ex_table *table)
{
	// NOTE: no fields in subtable
	if (table->nr_fields) {
		buffer_write_int32(out, table->nr_fields);
		for (uint32_t i = 0; i < table->nr_fields; i++) {
			write_field(out, &table->fields[i]);
		}
	}

	write_rows(out, table);
}

static void write_list(struct buffer *out, struct ex_list *list)
{
	buffer_write_int32(out, list->nr_items);
	for (uint32_t i = 0; i < list->nr_items; i++) {
		buffer_write_int32(out, list->items[i].value.type);
		size_t size_loc = skip_int32(out);
		size_t data_loc = out->index;
		_write_value(out, &list->items[i].value);
		buffer_write_int32_at(out, size_loc, out->index - data_loc);
	}
}

static void write_tree(struct buffer *out, struct ex_tree *tree)
{
	write_string(out, tree->name);
	buffer_write_int32(out, tree->is_leaf ? 1 : 0);

	if (tree->is_leaf) {
		buffer_write_int32(out, tree->leaf.value.type);
		size_t size_loc = skip_int32(out);
		size_t data_loc = out->index;
		write_string(out, tree->leaf.name);
		_write_value(out, &tree->leaf.value);
		buffer_write_int32_at(out, size_loc, out->index - data_loc);
		buffer_write_int32(out, 0);
	} else {
		buffer_write_int32(out, tree->nr_children);
		for (uint32_t i = 0; i < tree->nr_children; i++) {
			write_tree(out, &tree->children[i]);
		}
	}
}

static void write_block(struct buffer *out, struct ex_block *blk)
{
	buffer_write_int32(out, blk->val.type);
	size_t size_loc = skip_int32(out);
	size_t data_loc = out->index;
	write_string(out, blk->name);

	switch (blk->val.type) {
	case EX_INT:
		buffer_write_int32(out, blk->val.i);
		break;
	case EX_FLOAT:
		buffer_write_float(out, blk->val.f);
		break;
	case EX_STRING:
		write_string(out, blk->val.s);
		break;
	case EX_TABLE:
		write_table(out, blk->val.t);
		break;
	case EX_LIST:
		write_list(out, blk->val.list);
		break;
	case EX_TREE:
		write_tree(out, blk->val.tree);
		break;
	}

	buffer_write_int32_at(out, size_loc, out->index - data_loc);
}

void ex_compress(struct buffer *out, size_t len, size_t *len_out)
{
	unsigned long dst_len = len * 1.001 + 12;
	uint8_t *dst = xmalloc(dst_len);

	int r = compress2(dst, &dst_len, out->buf+out->index, len, 1);
	if (r != Z_OK) {
		ERROR("compress failed");
	}

	buffer_write_bytes(out, dst, dst_len);
	*len_out = dst_len;
	free(dst);
}

uint8_t *ex_flatten(struct ex *ex, size_t *size_out)
{
	struct buffer out;

	buffer_init(&out, xmalloc(128), 128);
	buffer_write_bytes(&out, (uint8_t*)"HEAD", 4);
	buffer_write_int32(&out, 0xc); // ???
	buffer_write_bytes(&out, (uint8_t*)"EXTF", 4);
	buffer_write_int32(&out, 0x1); // ???
	buffer_write_int32(&out, ex->nr_blocks);
	buffer_write_bytes(&out, (uint8_t*)"DATA", 4);
	size_t compressed_size_loc = skip_int32(&out);
	size_t uncompressed_size_loc = skip_int32(&out);
	size_t data_loc = out.index;

	// NOTE: everything after this point needs to be compressed and then
	//       encrypted before writing to disk.
	for (uint32_t i = 0; i < ex->nr_blocks; i++) {
		write_block(&out, &ex->blocks[i]);
	}

	size_t compressed_size;
	size_t uncompressed_size = out.index - data_loc;
	buffer_write_int32_at(&out, uncompressed_size_loc, uncompressed_size);

	buffer_seek(&out, data_loc);
	ex_compress(&out, uncompressed_size, &compressed_size);
	buffer_write_int32_at(&out, compressed_size_loc, compressed_size);

	*size_out = data_loc + compressed_size;
	return out.buf;
}

void ex_write(FILE *out, struct ex *ex)
{
	size_t size;
	uint8_t *flat = ex_flatten(ex, &size);

	ex_encode(flat+32, size - 32);

	if (fwrite(flat, size, 1, out) != 1)
		ERROR("Failed to write ex file: %s", strerror(errno));
	if (fclose(out))
		ERROR("Failed to close ex file: %s", strerror(errno));

	free(flat);
}
