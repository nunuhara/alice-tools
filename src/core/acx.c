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
#include <errno.h>
#include <zlib.h>

#include "little_endian.h"
#include "system4.h"
#include "system4/acx.h"
#include "system4/buffer.h"
#include "system4/string.h"

#include "alice.h"
#include "alice/acx.h"

// TODO: this should write to a buffer instead of a file, and there should be a
//       separate wrapper function which writes to a file
void acx_write(FILE *out, struct acx *acx)
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

static void write_string(FILE *out, const char *str)
{
	char *u = conv_output(str);

	fputc('"', out);
	for (int i = 0; u[i]; i++) {
		switch (u[i]) {
		case '"':  fprintf(out, "\\\""); break;
		case '\n': fprintf(out, "\\n");  break;
		default:   fputc(u[i], out);     break;
		}
	}
	fputc('"', out);

	free(u);
}

void acx_dump(FILE *out, struct acx *acx)
{
	for (int col = 0; col < acx->nr_columns; col++) {
		if (col > 0)
			fputc(',', out);
		switch (acx->column_types[col]) {
		case ACX_INT:    fprintf(out, "int"); break;
		case ACX_STRING: fprintf(out, "string"); break;
		default:
			WARNING("Unknown column type: %d", acx->column_types[col]);
			fprintf(out, "%d", acx->column_types[col]);
			break;
		}
	}
	fputc('\n', out);

	for (int line = 0; line < acx->nr_lines; line++) {
		for (int col = 0; col < acx->nr_columns; col++) {
			if (col > 0)
				fputc(',', out);
			if (acx->column_types[col] == ACX_STRING) {
				write_string(out, acx_get_string(acx, line, col)->text);
			} else {
				fprintf(out, "%d", acx_get_int(acx, line, col));
			}
		}
		fputc('\n', out);
	}
}
