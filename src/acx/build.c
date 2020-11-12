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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <unistd.h>
#include <zlib.h>
#include "little_endian.h"
#include "system4.h"
#include "system4/acx.h"
#include "system4/buffer.h"
#include "system4/file.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "alice.h"

enum {
	LOPT_OUTPUT = 256,
};

// csv_parser.y
extern struct acx *acx_parse(const char *path);
extern unsigned long csv_line;

static void acx_write(FILE *out, struct acx *acx)
{
	if (fwrite("ACX\0\0\0\0", 8, 1, out) != 1)
		ERROR("fwrite: %s", strerror(errno));

	unsigned long size = 8 + acx->nr_columns*4;
	for (int line = 0; line < acx->nr_lines; line++) {
		for (int col = 0; col < acx->nr_columns; col++) {
			if (acx->column_types[col] == ACX_STRING) {
				size += acx->lines[line*acx->nr_columns + col].s->size + 1;
			} else {
				size += 4;
			}
		}
	}

	// serialize
	struct buffer buf;
	buffer_init(&buf, NULL, 0);

	buffer_write_int32(&buf, acx->nr_columns);
	for (int col = 0; col < acx->nr_columns; col++) {
		buffer_write_int32(&buf, acx->column_types[col]);
	}
	buffer_write_int32(&buf, acx->nr_lines);

	for (int line = 0; line < acx->nr_lines; line++) {
		for (int col = 0; col < acx->nr_columns; col++) {
			if (acx->column_types[col] == ACX_STRING) {
				buffer_write_string(&buf, acx->lines[line*acx->nr_columns + col].s);
			} else {
				buffer_write_int32(&buf, acx->lines[line*acx->nr_columns + col].i);
			}
		}
	}

	// compress serialized data
	unsigned long compressed_size = buf.index * 1.001 + 12;
	uint8_t *dst = xmalloc(compressed_size);
	int r = compress2(dst, &compressed_size, buf.buf, buf.index, 1);
	if (r != Z_OK) {
		ERROR("compress failed");
	}

	// write data header
	uint8_t header[8];
	LittleEndian_putDW(header, 0, compressed_size);
	LittleEndian_putDW(header, 4, buf.index);
	if (fwrite(header, 8, 1, out) != 1)
		ERROR("fwrite: %s", strerror(errno));

	// write compressed data
	if (fwrite(dst, compressed_size, 1, out) != 1)
		ERROR("fwrite: %s", strerror(errno));

	fflush(out);
	free(buf.buf);
	free(dst);
}

int command_acx_build(int argc, char *argv[])
{
	const char *output_file = NULL;
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_acx_build);
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

	if (argc != 1) {
		USAGE_ERROR(&cmd_acx_build, "Wrong number of arguments.");
	}

	current_line_nr = &csv_line;
	current_file_name = (const char**)&argv[0];

	FILE *out = alice_open_output_file(output_file);
	struct acx *acx = acx_parse(argv[0]);
	acx_write(out, acx);
	acx_free(acx);
	fclose(out);
	return 0;
}

struct command cmd_acx_build = {
	.name = "build",
	.usage = "[options...] <input-file>",
	.description = "Build a .acx file from a .csv",
	.parent = &cmd_acx,
	.fun = command_acx_build,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
