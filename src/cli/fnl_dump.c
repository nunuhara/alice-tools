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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <webp/encode.h>
#include <png.h>
#include "little_endian.h"
#include "system4.h"
#include "system4/file.h"
#include "system4/fnl.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "alice.h"
#include "cli.h"

static void write_bitmap(FILE *f, uint32_t width, uint32_t height, uint8_t *pixels)
{
	if (width % 8 != 0)
		ERROR("Invalid glyph width");

	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_byte **row_pointers = NULL;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		ERROR("png_create_write_struct failed");

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		ERROR("png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr)))
		ERROR("png_init_io failed");

	png_init_io(png_ptr, f);

	if (setjmp(png_jmpbuf(png_ptr)))
		ERROR("png_write_header failed");

	png_set_IHDR(png_ptr, info_ptr, width, height, 1,
		     PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, info_ptr);

	uint32_t stride = width / 8;
	row_pointers = png_malloc(png_ptr, height * sizeof(png_byte*));
	for (unsigned i = 0; i < height; i++) {
		row_pointers[height - (i+1)] = pixels + i*stride;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
		ERROR("png_write_image failed");

	png_write_image(png_ptr, row_pointers);

	if (setjmp(png_jmpbuf(png_ptr)))
		ERROR("png_write_end failed");

	png_write_end(png_ptr, NULL);

	png_free(png_ptr, row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
}

enum {
	LOPT_OUTPUT = 256,
};

int command_fnl_dump(int argc, char *argv[])
{
	char path[PATH_MAX];
	char *output_dir = ".";
	while (1) {
		int c = alice_getopt(argc, argv, &cmd_fnl_dump);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_dir = optarg;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		USAGE_ERROR(&cmd_fnl_dump, "Wrong number of arguments");
	}

	struct fnl *fnl = fnl_open(argv[0]);
	if (!fnl)
		ALICE_ERROR("fnl_open failed");

	for (size_t font = 0; font < fnl->nr_fonts; font++) {
		NOTICE("FONT %u", (unsigned)font);
		for (size_t face = 0; face < fnl->fonts[font].nr_faces; face++) {
			struct fnl_font_face *font_face = &fnl->fonts[font].faces[face];
			NOTICE("\tsize %lu (%lu glyphs)", font_face->height, font_face->nr_glyphs);

			// create subdir
			sprintf(path, "%s/font_%u/%upx", output_dir, (unsigned)font, (unsigned)font_face->height);
			mkdir_p(path);

			// extract glyphs
			for (size_t g = 0; g < font_face->nr_glyphs; g++) {
				struct fnl_glyph *glyph = &font_face->glyphs[g];
				if (!glyph->data_pos)
					continue;

				sprintf(path, "%s/font_%u/%upx/glyph_%u.png", output_dir, (unsigned)font,
					(unsigned)font_face->height, (unsigned)g);
				FILE *f = file_open_utf8(path, "wb");
				if (!f) {
					ERROR("fopen failed: %s", strerror(errno));
				}

				unsigned long data_size;
				uint8_t *data = fnl_glyph_data(fnl, glyph, &data_size);

				uint32_t height = font_face->height;
				uint32_t width = (data_size*8) / height;
				write_bitmap(f, width, height, data);
				free(data);
				fclose(f);
			}
		}
	}
	fnl_free(fnl);
	return 0;
}

struct command cmd_fnl_dump = {
	.name = "dump",
	.usage = "[options] <input-file>",
	.description = "Unpack AliceSoft font library files (.fnl)",
	.parent = &cmd_fnl,
	.fun = command_fnl_dump,
	.options = {
		{ "output", 'o', "Specify the output directory", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
