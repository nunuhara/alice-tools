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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "system4.h"
#include "system4/buffer.h"
#include "system4/cg.h"
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/flat.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ex.h"
#include "alice/flat.h"

static void buffer_write_file(struct buffer *buf, const char *path)
{
	size_t len;
	uint8_t *data = file_read(path, &len);
	if (!data)
		ALICE_ERROR("reading '%s': %s", path, strerror(errno));
	buffer_write_bytes(buf, data, len);
	free(data);
}

static struct string *get_path(const struct string *dir, const char *file)
{
	char *ufile = conv_output_utf8(file);
	struct string *path = string_path_join(dir, ufile);
	free(ufile);
	return path;
}

static void buffer_write_file_relative(struct buffer *buf, const struct string *dir, const char *name)
{
	struct string *path = get_path(dir, name);
	buffer_write_file(buf, path->text);
	free_string(path);
}

static void pad_align(struct buffer *b)
{
	while (b->index & 3) {
		uint8_t zero = 0;
		buffer_write_bytes(b, &zero, 1);
	}
}

static void deserialize_binary(struct buffer *b, struct string *s)
{
	if (s->size % 2 != 0)
		ALICE_ERROR("Serialized binary data has odd size");

	size_t off = b->index;
	buffer_skip(b, 4);

	for (int i = 0; i < s->size; i += 2) {
		uint8_t n;
		sscanf(s->text+i, "%02hhx", &n);
		buffer_write_bytes(b, &n, 1);
	}

	buffer_write_int32_at(b, off, b->index - off - 4);
	pad_align(b);
}

static void write_libl_files(struct buffer *b, struct ex_table *libl, const struct string *dir)
{
	// validate fields
	if (libl->nr_fields != 5)
		ALICE_ERROR("Wrong number of columns in 'libl' table");
	if (libl->fields[0].type != EX_STRING)
		ALICE_ERROR("Wrong type for column 'unknown' in 'libl' table");
	if (libl->fields[1].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'type' in 'libl' table");
	if (libl->fields[2].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'has_front' in 'libl' table");
	if (libl->fields[3].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'front' in 'libl' table");
	if (libl->fields[4].type != EX_STRING)
		ALICE_ERROR("Wrong type");

	buffer_write_int32(b, libl->nr_rows);

	for (unsigned i = 0; i < libl->nr_rows; i++) {
		deserialize_binary(b, libl->rows[i][0].s);
		buffer_write_int32(b, libl->rows[i][1].i);

		struct string *path = get_path(dir, libl->rows[i][4].s->text);
		//const char *path = libl->rows[i][4].s->text;
		if (libl->rows[i][2].i) {
			buffer_write_int32(b, file_size(path->text) + 4);
			buffer_write_int32(b, libl->rows[i][3].i);
		} else {
			buffer_write_int32(b, file_size(path->text));
		}
		buffer_write_file(b, path->text);
		free_string(path);
		pad_align(b);
	}
}

static void write_talt_files(struct buffer *b, struct ex_table *talt, const struct string *dir)
{
	// validate fields
	if (talt->nr_fields != 2)
		ALICE_ERROR("Wrong number of columns in 'talt' table");
	if (talt->fields[0].type != EX_STRING)
		ALICE_ERROR("Wrong type for column 'unknown' in 'talt' table");
	if (talt->fields[1].type != EX_TABLE)
		ALICE_ERROR("Wrong type for column 'meta' in 'talt' table");
	if (talt->fields[1].nr_subfields != 5)
		ALICE_ERROR("Wrong number of columns in 'talt.meta' table");
	if (talt->fields[1].subfields[0].type != EX_STRING)
		ALICE_ERROR("Wrong type for column 'uk1' in 'talt.meta' table");
	if (talt->fields[1].subfields[1].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'uk2' in 'talt.meta' table");
	if (talt->fields[1].subfields[2].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'uk3' in 'talt.meta' table");
	if (talt->fields[1].subfields[3].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'uk4' in 'talt.meta' table");
	if (talt->fields[1].subfields[4].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'uk5' in 'talt.meta' table");

	buffer_write_int32(b, talt->nr_rows);

	for (unsigned i = 0; i < talt->nr_rows; i++) {
		struct string *path = get_path(dir, talt->rows[i][0].s->text);
		buffer_write_int32(b, file_size(path->text));
		buffer_write_file(b, path->text);
		free_string(path);
		pad_align(b);

		struct ex_table *meta = talt->rows[i][1].t;
		buffer_write_int32(b, meta->nr_rows);
		for (unsigned i = 0; i < meta->nr_rows; i++) {
			deserialize_binary(b, meta->rows[i][0].s);
			buffer_write_int32(b, meta->rows[i][1].i);
			buffer_write_int32(b, meta->rows[i][2].i);
			buffer_write_int32(b, meta->rows[i][3].i);
			buffer_write_int32(b, meta->rows[i][4].i);
		}
	}
}

static struct flat_archive *build_flat(struct ex *ex, const struct string *dir)
{
	struct buffer b;
	struct flat_archive *flat = flat_new();
	buffer_init(&b, NULL, 0);

	if (ex_get_int(ex, "elna", 0)) {
		flat->elna.present = true;
		flat->elna.off = b.index;
		flat->elna.size = 0;
		buffer_write_bytes(&b, (uint8_t*)"ELNA", 4);
		buffer_write_int32(&b, 0);
	}

	struct string *flat_path = ex_get_string(ex, "flat");
	if (!flat_path)
		ALICE_ERROR("'flat' path missing from .flat manifest");
	flat->flat.present = true;
	flat->flat.off = b.index;
	buffer_write_file_relative(&b, dir, flat_path->text);
	flat->flat.size = b.index - flat->flat.off - 8;
	free_string(flat_path);

	struct string *tmnl_path = ex_get_string(ex, "tmnl");
	if (tmnl_path) {
		flat->tmnl.present = true;
		flat->tmnl.off = b.index;
		buffer_write_file_relative(&b, dir, tmnl_path->text);
		flat->tmnl.size = b.index - flat->tmnl.off - 8;
		free_string(tmnl_path);
	}

	struct string *mtlc_path = ex_get_string(ex, "mtlc");
	if (!mtlc_path)
		ALICE_ERROR("'mtlc' path missing from .flat manifest");
	flat->mtlc.present = true;
	flat->mtlc.off = b.index;
	buffer_write_file_relative(&b, dir, mtlc_path->text);
	flat->mtlc.size = b.index - flat->mtlc.off - 8;
	free_string(mtlc_path);

	struct ex_table *libl = ex_get_table(ex, "libl");
	if (!libl)
		ALICE_ERROR("'libl' table missing from .flat manifest");
	flat->libl.present = true;
	flat->libl.off = b.index;
	buffer_write_bytes(&b, (uint8_t*)"LIBL", 4);
	buffer_write_int32(&b, 0);
	write_libl_files(&b, libl, dir);
	// write the section size now that we know it
	buffer_write_int32_at(&b, flat->libl.off + 4, b.index - flat->libl.off - 8);

	struct ex_table *talt = ex_get_table(ex, "talt");
	if (talt) {
		flat->talt.present = true;
		flat->talt.off = b.index;
		buffer_write_bytes(&b, (uint8_t*)"TALT", 4);
		buffer_write_int32(&b, 0);
		write_talt_files(&b, talt, dir);
		// write the section size now that we know it
		buffer_write_int32_at(&b, flat->talt.off + 4, b.index - flat->talt.off - 8);
	}

	flat->data_size = b.index;
	flat->data = b.buf;
	flat->needs_free = true;
	return flat;
}

struct flat_archive *flat_build(const char *xpath, struct string **output_path)
{
	struct string *dir = cstr_to_string(path_dirname(xpath));
	struct ex *ex = ex_parse_file(xpath);
	if (!ex) {
		ALICE_ERROR("Failed to read flat manifest file: %s", xpath);
	}

	if (output_path) {
		// FIXME: this sucks
		struct string *tmp = ex_get_string(ex, "output");
		if (tmp) {
			char *utmp = conv_output_utf8(tmp->text);
			*output_path = cstr_to_string(utmp);
			free(utmp);
			free_string(tmp);
		}
	}

	struct flat_archive *flat = build_flat(ex, dir);
	free_string(dir);
	ex_free(ex);
	return flat;
}

static const char *get_extension(enum flat_data_type t, uint8_t *data)
{
	switch (t) {
	case FLAT_CG:
		return cg_file_extension(cg_check_format(data));
	case FLAT_ZLIB:
		return "z";
	}
	return "dat";
}

static const char *libl_get_extension(struct flat_archive *flat, struct libl_entry *e)
{
	return get_extension(e->type, flat->data + e->off);
}

static const char *talt_get_extension(struct flat_archive *flat, struct talt_entry *e)
{
	return get_extension(FLAT_CG, flat->data + e->off);
}

static char *serialize_bytes(const uint8_t *b, size_t size)
{
	char *out = xmalloc(size * 2 + 1);
	for (unsigned i = 0; i < size; i++) {
		sprintf(out + i*2, "%02x", (unsigned)b[i]);
	}
	out[size*2] = '\0';
	return out;
}

static void write_file(const char *path, void *data, size_t size, possibly_unused enum flat_data_type type)
{
	FILE *f = checked_fopen(path, "wb");
	if (type == FLAT_CG) {
		// TODO: convert to PNG
		checked_fwrite(data, size, f);
	} else {
		checked_fwrite(data, size, f);
	}
	fclose(f);
}

static void write_section(const char *path, struct flat_archive *flat, struct flat_section *section)
{
	write_file(path, flat->data + section->off, section->size + 8, 0);
}

void flat_extract(struct flat_archive *flat, const char *output_file)
{
	FILE *out = checked_fopen(output_file, "wb");
	char *prefix = escape_string_noconv(output_file);
	char path_buf[PATH_MAX];

	// ELNA section
	fprintf(out, "int elna = %d;\n\n", flat->elna.present ? 1 : 0);

	// FLAT section
	fprintf(out, "string flat = \"%s.head\";\n\n", prefix);
	snprintf(path_buf, PATH_MAX-1, "%s.head", output_file);
	write_section(path_buf, flat, &flat->flat);

	// TMNL section
	if (flat->tmnl.present) {
		fprintf(out, "string tmnl = \"%s.tmnl\";\n\n", prefix);
		snprintf(path_buf, PATH_MAX-1, "%s.tmnl", output_file);
		write_section(path_buf, flat, &flat->tmnl);
	}

	// MTLC section
	fprintf(out, "string mtlc = \"%s.mtlc\";\n\n", prefix);
	snprintf(path_buf, PATH_MAX-1, "%s.mtlc", output_file);
	write_section(path_buf, flat, &flat->mtlc);

	// LIBL section
	fprintf(out, "table libl = {\n");
	fprintf(out, "\t{ string unknown, int type, int has_front, int front, string path },\n");
	for (unsigned i = 0; i < flat->nr_libl_entries; i++) {
		struct libl_entry *e = &flat->libl_entries[i];
		const char *ext = libl_get_extension(flat, e);
		char *uk = serialize_bytes(flat->data + e->unknown_off, e->unknown_size);
		fprintf(out, "\t{ \"%s\", %d, %d, %d, \"%s.libl.%d.%s\" },\n",
			uk, e->type, e->has_front_pad, e->front_pad, prefix, i, ext);
		free(uk);

		// write file
		snprintf(path_buf, PATH_MAX-1, "%s.libl.%d.%s", output_file, i, ext);
		write_file(path_buf, flat->data + e->off, e->size, e->type);
	}
	fprintf(out, "};\n");

	// TALT section
	if (flat->talt.present) {
		fprintf(out, "\ntable talt = {\n");
		fprintf(out, "\t{ string path, table meta { string uk1, int uk2, int uk3, int uk4, int uk5 }},\n");
		for (unsigned i = 0; i < flat->nr_talt_entries; i++) {
			struct talt_entry *e = &flat->talt_entries[i];
			const char *ext = talt_get_extension(flat, e);
			fprintf(out, "\t{ \"%s.talt.%d.%s\", {\n", prefix, i, ext);
			for (unsigned j = 0; j < e->nr_meta; j++) {
				struct talt_metadata *m = &e->metadata[j];
				char *uk = serialize_bytes(flat->data + m->unknown1_off, m->unknown1_size);
				fprintf(out, "\t\t{ \"%s\", %d, %d, %d, %d },\n",
				        uk, m->unknown2, m->unknown3, m->unknown4, m->unknown5);
				free(uk);
			}
			fprintf(out, "\t}},\n");

			// write file
			snprintf(path_buf, PATH_MAX-1, "%s.talt.%d.%s", output_file, i, ext);
			write_file(path_buf, flat->data + e->off, e->size, FLAT_CG);
		}
		fprintf(out, "};\n");
	}

	free(prefix);
	fclose(out);
}
