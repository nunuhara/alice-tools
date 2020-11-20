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
#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <limits.h>
#include <libgen.h>
#include <png.h>
#include <webp/encode.h>
#include "system4.h"
#include "system4/afa.h"
#include "system4/ald.h"
#include "system4/cg.h"
#include "system4/file.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "alice.h"
#include "archive.h"

enum {
	LOPT_HELP = 256,
	LOPT_OUTPUT,
	LOPT_INDEX,
	LOPT_NAME,
	LOPT_FORCE,
	LOPT_IMAGE_FORMAT,
	LOPT_IMAGES_ONLY,
	LOPT_RAW,
};

static bool raw = false;
static bool force = false;
static bool images_only = false;

// Add a trailing slash to a path
char *output_file_dir(const char *path)
{
	if (!path || *path == '\0')
		return strdup("./"); // default is cwd

	size_t size = strlen(path);
	if (path[size-1] == '/')
		return strdup(path);

	char *s = xmalloc(size + 2);
	strcpy(s, path);
	s[size] = '/';
	s[size+1] = '\0';
	return s;
}

static void mkdir_for_file(const char *filename)
{
	char *tmp = strdup(filename);
	char *dir = dirname(tmp);
	mkdir_p(dir);
	free(tmp);
}

static void write_webp(struct cg *cg, FILE *f)
{
	uint8_t *out;
	size_t len = WebPEncodeLosslessRGBA(cg->pixels, cg->metrics.w, cg->metrics.h, cg->metrics.w*4, &out);
	if (!fwrite(out, len, 1, f))
		ERROR("fwrite failed: %s", strerror(errno));
	WebPFree(out);
}

static void write_png(struct cg *cg, FILE *f)
{
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_byte **row_pointers = NULL;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		WARNING("png_create_write_struct failed");
		goto cleanup;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		WARNING("png_create_info_struct failed");
		goto cleanup;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		WARNING("png_init_io failed");
		goto cleanup;
	}

	png_init_io(png_ptr, f);

	if (setjmp(png_jmpbuf(png_ptr))) {
		WARNING("png_write_header failed");
		goto cleanup;
	}

	png_set_IHDR(png_ptr, info_ptr, cg->metrics.w, cg->metrics.h,
		     8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, info_ptr);

	png_uint_32 stride = cg->metrics.w * 4;
	row_pointers = png_malloc(png_ptr, cg->metrics.h * sizeof(png_byte*));
	for (int i = 0; i < cg->metrics.h; i++) {
		row_pointers[i] = cg->pixels + i*stride;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		WARNING("png_write_image failed");
		goto cleanup;
	}

	png_write_image(png_ptr, row_pointers);

	if (setjmp(png_jmpbuf(png_ptr))) {
		WARNING("png_write_end failed");
		goto cleanup;
	}

	png_write_end(png_ptr, NULL);
cleanup:
	if (row_pointers)
		png_free(png_ptr, row_pointers);
	if (png_ptr)
		png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : NULL);
	return;
}

enum imgenc {
	IMGENC_PNG,
	IMGENC_WEBP,
};

struct image_encoder {
	void (*write)(struct cg *cg, FILE *f);
	const char * const ext;
};

static struct image_encoder image_encoders[] = {
	[IMGENC_PNG]  = { write_png,  ".png"  },
	[IMGENC_WEBP] = { write_webp, ".webp" },
};

static enum imgenc imgenc = IMGENC_PNG;

static bool is_image_file(struct archive_data *data)
{
	return cg_check_format(data->data) != ALCG_UNKNOWN;
}

static char *get_default_filename(struct archive_data *data, const char *ext)
{
	char *u = conv_output(data->name);
	if (ext) {
		size_t ulen = strlen(u);
		size_t extlen = strlen(ext);
		u = xrealloc(u, ulen + extlen + 1);
		memcpy(u+ulen, ext, extlen + 1);
	}
	return u;
}

static bool write_file(struct archive_data *data, const char *output_file)
{
	FILE *f = NULL;
	bool output_img = !raw && is_image_file(data);

	if (!output_file) {
		char *u = get_default_filename(data, output_img ? image_encoders[imgenc].ext : NULL);
		mkdir_for_file(u);
		if (!force && file_exists(u)) {
			free(u);
			return false;
		}
		if (!(f = checked_fopen(u, "wb"))) {
			free(u);
			return false;
		}
		free(u);
	} else if (strcmp(output_file, "-")) {
		if (!force && file_exists(output_file)) {
			return false;
		}
		if (!(f = checked_fopen(output_file, "wb"))) {
			return false;
		}
	}

	if (output_img) {
		struct cg *cg = cg_load_data(data);
		if (cg) {
			image_encoders[imgenc].write(cg, f);
			cg_free(cg);
		} else {
			WARNING("Failed to load CG");
		}
	} else if (!images_only && fwrite(data->data, data->size, 1, f ? f : stdout) != 1) {
		ERROR("fwrite failed: %s", strerror(errno));
	}

	if (f)
		fclose(f);
	return true;
}

static void extract_all_iter(struct archive_data *data, void *_output_dir);

static void extract_flat(struct archive_data *data, char *output_dir)
{
	int error;
	struct archive *ar = flat_open(data->data, data->size, &error);
	if (!ar) {
		WARNING("Error opening FLAT archive: %s", archive_strerror(error));
		return;
	}

	// generate filename prefix
	char *uname = conv_output(data->name);
	size_t dir_len = strlen(output_dir);
	size_t name_len = strlen(uname);
	char *prefix = xmalloc(dir_len + name_len + 2);
	strcpy(prefix, output_dir);
	strcpy(prefix+dir_len, uname);
	strcpy(prefix+dir_len+name_len, ".");
	//NOTICE("Extracting %s...", uname);

	archive_for_each(ar, extract_all_iter, prefix);
	archive_free(ar);
	free(prefix);
	free(uname);
}

static bool is_flat_file(const char *data)
{
	if (!strncmp(data, "ELNA", 4))
		data += 8;
	return !strncmp(data, "FLAT", 4);
}

static void extract_all_iter(struct archive_data *data, void *_prefix)
{
	if (!archive_load_file(data)) {
		char *u = conv_output(data->name);
		WARNING("Error loading file: %s", u);
		free(u);
		return;
	}

	if (!raw && is_flat_file((char*)data->data)) {
		char *u = conv_output(data->name);
		NOTICE("Extracting %s...", u);
		free(u);
		extract_flat(data, _prefix);
		return;
	}

	char *prefix = _prefix;
	bool is_image = is_image_file(data);
	char *file_name = get_default_filename(data, !raw && is_image ? image_encoders[imgenc].ext : NULL);
	char output_file[PATH_MAX];
	snprintf(output_file, PATH_MAX, "%s%s", prefix, file_name);
	free(file_name);
	if (!is_image && images_only) {
		NOTICE("Skipping non-image file: %s", output_file);
		return;
	}

	mkdir_for_file(output_file);

	if (write_file(data, output_file))
		NOTICE("%s", output_file);
	else
		NOTICE("Skipping existing file: %s", output_file);
}

int command_ar_extract(int argc, char *argv[])
{
	char *output_file = NULL;
	possibly_unused char *input_file = NULL;
	char *file_name = NULL;
	int file_index = -1;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ar_extract);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case 'i':
		case LOPT_INDEX:
			// FIXME: use strtol with error checking
			file_index = atoi(optarg);
			break;
		case 'n':
		case LOPT_NAME:
			file_name = optarg;
			break;
		case 'f':
		case LOPT_FORCE:
			force = true;
			break;
		case LOPT_IMAGE_FORMAT:
			if (!strcasecmp(optarg, "png"))
				imgenc = IMGENC_PNG;
			else if (!strcasecmp(optarg, "webp"))
				imgenc = IMGENC_WEBP;
			else
				ERROR("Unrecognized image format: \"%s\"", optarg);
			break;
		case LOPT_IMAGES_ONLY:
			images_only = true;
			break;
		case LOPT_RAW:
			raw = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	// check argument count
	if (argc != 1) {
		USAGE_ERROR(&cmd_ar_extract, "Wrong number of arguments");
	}

	// open archive
	struct archive *ar;
	enum archive_type type;
	int error;
	ar = open_archive(argv[0], &type, &error);
	if (!ar) {
		ERROR("Opening archive: %s", archive_strerror(error));
	}

	// run command
	if (file_index >= 0) {
		struct archive_data *d = archive_get(ar, file_index);
		if (!d)
			ERROR("No file with index %d", file_index);
		write_file(d, output_file);
		archive_free_data(d);
	} else if (file_name) {
		char *u = utf2sjis(file_name, strlen(file_name));
		struct archive_data *d = archive_get_by_name(ar, u);
		if (!d)
			ERROR("No file with name \"%s\"", u);
		write_file(d, output_file);
		archive_free_data(d);
		free(u);
	} else {
		output_file = output_file_dir(output_file);
		archive_for_each(ar, extract_all_iter, output_file);
		free(output_file);
	}

	archive_free(ar);
	return 0;
}

struct command cmd_ar_extract = {
	.name = "extract",
	.usage = "[options...] <input-file>",
	.description = "Extract an archive file",
	.parent = &cmd_ar,
	.fun = command_ar_extract,
	.options = {
		{ "output",       'o', "Specify output file/directory",     required_argument, LOPT_OUTPUT },
		{ "index",        'i', "Specify file index",                required_argument, LOPT_INDEX },
		{ "name",         'n', "Specify file name",                 required_argument, LOPT_NAME },
		{ "force",        'f', "Allow overwriting existing files",  no_argument,       LOPT_FORCE },
		{ "image-format", 0,   "Image output format (png or webp)", required_argument, LOPT_IMAGE_FORMAT },
		{ "images-only",  0,   "Only extract images",               no_argument,       LOPT_IMAGES_ONLY },
		{ "raw",          0,   "Don't convert image files",         no_argument,       LOPT_RAW },
		{ 0 }
	}
};
