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
#include <zlib.h>
#include "system4.h"
#include "system4/archive.h"
#include "system4/buffer.h"
#include "system4/file.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "alice.h"
#include "alice-ar.h"

static uint32_t align8(uint32_t i)
{
	return (i+7) & ~7;
}

static uint8_t zpad[0x1000] = {0};

void write_afa(struct string *filename, struct ar_file_spec **files, size_t nr_files)
{
	// open output file
	FILE *f = checked_fopen(filename->text, "wb");

	// get file sizes
	off_t *sizes = xcalloc(nr_files, sizeof(off_t));
	size_t data_size = 0;
	for (size_t i = 0; i < nr_files; i++) {
		sizes[i] = file_size(files[i]->path->text);
		if (sizes[i] <= 0) {
			ALICE_ERROR("can't determine size of file: %s", files[i]->path->text);
		}
		data_size += align8(sizes[i]);
	}

	// write index to buffer
	off_t off = 8;
	struct buffer buf;
	buffer_init(&buf, NULL, 0);
	for (size_t i = 0; i < nr_files; i++) {
#ifdef _WIN32
		char *u = strdup(files[i]->name->text);
#else
		char *u = utf2sjis(files[i]->name->text, files[i]->name->size);
#endif
		buffer_write_int32(&buf, strlen(u));
		buffer_write_pascal_cstring(&buf, u);
		buffer_write_int32(&buf, 0); // timestamp?
		buffer_write_int32(&buf, 0); // timestamp?
		buffer_write_int32(&buf, off);
		buffer_write_int32(&buf, sizes[i]);
		off += align8(sizes[i]);
		free(u);
	}

	// compress index
	unsigned long uncompressed_size = buf.index;
	unsigned long file_table_len = uncompressed_size * 1.001 + 12;
	uint8_t *file_table = xmalloc(file_table_len);
	int r = compress2(file_table, &file_table_len, buf.buf, uncompressed_size, 1);
	if (r != Z_OK) {
		ALICE_ERROR("compress2 failed");
	}

	// XXX: ALDExplorer won't open archive unless data_start is aligned to 0x1000.
	//      On the other hand, AliceSoft aligns to 1MB (???)
	size_t data_start = (44 + file_table_len + 0xFFF) & ~0xFFF;
	size_t pad = data_start - (44 + file_table_len);

	// write header to buffer
	buf.index = 0;
	buffer_write_bytes(&buf, (uint8_t*)"AFAH", 4);
	buffer_write_int32(&buf, 0x1c);
	buffer_write_bytes(&buf, (uint8_t*)"AlicArch", 8);
	buffer_write_int32(&buf, 2);
	buffer_write_int32(&buf, 1); // ???
	buffer_write_int32(&buf, data_start);
	buffer_write_bytes(&buf, (uint8_t*)"INFO", 4);
	buffer_write_int32(&buf, file_table_len + 16);
	buffer_write_int32(&buf, uncompressed_size);
	buffer_write_int32(&buf, nr_files);

	// write header to archive
	checked_fwrite(buf.buf, buf.index, f);

	// write index to archive
	checked_fwrite(file_table, file_table_len, f);
	free(file_table);

	// write padding to archive
	buf.index = 0;
	if (pad) {
		if (pad >= 8) {
			buffer_write_bytes(&buf, (uint8_t*)"DUMM", 4);
			buffer_write_int32(&buf, pad);
			pad -= 8;
		}
		buffer_write_bytes(&buf, zpad, pad);
	}

	// write files to archive
	buffer_write_bytes(&buf, (uint8_t*)"DATA", 4);
	buffer_write_int32(&buf, data_size+8);
	checked_fwrite(buf.buf, buf.index, f);
	for (size_t i = 0; i < nr_files; i++) {
		NOTICE("%s", files[i]->path->text);
		FILE *in = checked_fopen(files[i]->path->text, "rb");
		uint8_t *tmp = xmalloc(sizes[i]);
		checked_fread(tmp, sizes[i], in);
		checked_fwrite(tmp, sizes[i], f);
		free(tmp);
		fclose(in);

		int pad = align8(sizes[i]) - sizes[i];
		if (pad)
			checked_fwrite(zpad, pad, f);
	}

	fflush(f);
	fclose(f);
	free(buf.buf);
	free(sizes);
}
