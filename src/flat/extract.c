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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "system4.h"
#include "system4/cg.h"
#include "system4/flat.h"
#include "alice.h"

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

static char *get_output_path(const char *output_file, const char *input_file)
{
	if (output_file)
		return xstrdup(output_file);

	size_t len = strlen(input_file);
	char *name = xmalloc(len + 3);
	memcpy(name, input_file, len);
	memcpy(name+len, ".x", 3);
	return name;
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

static void flat_extract(struct flat_archive *flat, const char *output_file)
{
	FILE *out = checked_fopen(output_file, "wb");
	char *prefix = escape_string(output_file);
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

enum {
	LOPT_OUTPUT = 256,
};

int command_flat_extract(int argc, char *argv[])
{
	char *output_file = NULL;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_flat_extract);
		if (c == -1)
			break;
		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	// check argument count
	if (argc != 1) {
		USAGE_ERROR(&cmd_flat_extract, "Wrong number of arguments");
	}

	// open .flat file
	struct flat_archive *flat;
	int error;
	char *input_file = conv_cmdline_utf8(argv[0]);
	flat = flat_open_file(input_file, 0, &error);
	free(input_file);

	if (!flat) {
		ALICE_ERROR("Opening archive: %s", archive_strerror(error));
	}

	// write manifest
	output_file = get_output_path(output_file, argv[0]);
	flat_extract(flat, output_file);

	free(output_file);
	archive_free(&flat->ar);

	return 0;
}

struct command cmd_flat_extract = {
	.name = "extract",
	.usage = "[options...] <input-file>",
	.description = "Extract the contents of a .flat file",
	.parent = &cmd_flat,
	.fun = command_flat_extract,
	.options = {
		{ "output", 'o', "Specify output file", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
