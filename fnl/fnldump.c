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
#include "little_endian.h"
#include "system4.h"
#include "system4/file.h"
#include "system4/fnl.h"
#include "system4/string.h"
#include "system4/utfsjis.h"

static void usage(void)
{
	puts("Usage: fnldump [options...] input-file");
	puts("    Unpack AliceSoft font archive files (.fnl)");
	puts("");
	puts("    -h, --help                 Display this message and exit");
	puts("    -o, --output               Speficy the output directory");
}

void write_bitmap(FILE *f, uint32_t width, uint32_t height, uint8_t *pixels)
{
	// expand to RGB packed pixel
	uint8_t *rgb = xmalloc(width * height * 3);

	uint32_t p = 0;
	for (int row = height - 1; row >= 0; row--) {
		for (unsigned col = 0; col < width; col++, p++) {
			uint8_t v = pixels[p/8] & (1 << (7 - p % 8)) ? 255 : 0;
			rgb[row*width*3 + col*3 + 0] = v;
			rgb[row*width*3 + col*3 + 1] = v;
			rgb[row*width*3 + col*3 + 2] = v;
		}
	}

	uint8_t *output;
	size_t len = WebPEncodeLosslessRGB(rgb, width, height, width*3, &output);

	if (!fwrite(output, len, 1, f)) {
		ERROR("fwrite failed: %s", strerror(errno));
	}

	free(rgb);
	WebPFree(output);
	return;
}

enum {
	LOPT_HELP = 256,
	LOPT_OUTPUT,
};

int main(int argc, char *argv[])
{
	char path[PATH_MAX];
	char *output_dir = ".";
	while (1) {
		static struct option long_options[] = {
			{ "help",   no_argument,       0, LOPT_HELP },
			{ "output", required_argument, 0, LOPT_OUTPUT },
		};
		int option_index = 0;
		int c = getopt_long(argc, argv, "ho:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case LOPT_HELP:
			usage();
			return 0;
		case 'o':
		case LOPT_OUTPUT:
			output_dir = optarg;
			break;
		case '?':
			ERROR("Unrecognized command line argument");
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		ERROR("Wrong number of arguments");
	}

	struct fnl *fnl = fnl_open(argv[0]);
	for (size_t font = 0; font < fnl->nr_fonts; font++) {
		NOTICE("FONT %u", (unsigned)font);
		for (size_t face = 0; face < fnl->fonts[font].nr_faces; face++) {
			struct fnl_font_face *font_face = &fnl->fonts[font].faces[face];
			NOTICE("\tsize %lu (%lu glyphs)", font_face->height, font_face->nr_glyphs);

			// create subdir
			sprintf(path, "%s/font_%u/face_%u", output_dir, (unsigned)font, (unsigned)face);
			mkdir_p(path);

			// extract glyphs
			for (size_t g = 0; g < font_face->nr_glyphs; g++) {
				struct fnl_glyph *glyph = &font_face->glyphs[g];
				if (!glyph->data_pos)
					continue;

				sprintf(path, "%s/font_%u/face_%u/glyph_%u.webp", output_dir, (unsigned)font, (unsigned)face, (unsigned)g);
				FILE *f = fopen(path, "wb");
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
