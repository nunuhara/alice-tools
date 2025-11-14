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
#include <libgen.h>
#include <png.h>
#include <webp/encode.h>
#include "system4.h"
#include "system4/afa.h"
#include "system4/ald.h"
#include "system4/cg.h"
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/flat.h"
#include "system4/png.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "system4/webp.h"
#include "alice.h"
#include "alice/ar.h"
#include "cli.h"

enum {
	LOPT_HELP = 256,
	LOPT_OUTPUT,
	LOPT_INDEX,
	LOPT_NAME,
	LOPT_FORCE,
	LOPT_IMAGE_FORMAT,
	LOPT_IMAGES_ONLY,
	LOPT_RAW,
	LOPT_MANIFEST,
	LOPT_FLAT_PNG,
};

static bool raw = false;

static struct conv_format {
	const char *src;
	const char *dst;
	bool rev_conv_supported;
	bool warned;
} conv_formats[] = {
	{ "qnt",    "png", true },
	{ "ajp",    "png", false },
	{ "pms",    "png", false },
	{ "webp",   "png", false },
	{ "dcf",    "png", false },
	{ "pcf",    "png", false },
	{ "rou",    "png", false },
	{ "ex",     "x",   true },
	{ "pactex", "x",   true },
	{ "flat",   "x",   true },
};

static int is_conv_format(const char *ext)
{
	for (int i = 0; i < sizeof(conv_formats)/sizeof(*conv_formats); i++) {
		if (!strcasecmp(ext, conv_formats[i].src))
			return i;
	}
	return -1;
}

static struct string *append_extension(const char *name, const char *ext)
{
	struct string *s = make_string(name, strlen(name));
	string_append_cstr(&s, ".", 1);
	string_append_cstr(&s, ext, strlen(ext));
	return s;
}

static void write_manifest_iter(struct archive_data *data, void *_f)
{
	FILE *f = _f;
	char *name = conv_output(data->name);
	size_t len;
	for (len = 0; name[len]; len++) {
		if (name[len] == '\\')
			name[len] = '/';
	}

	int i;
	const char *ext = file_extension(name);
	if (!raw && (i = is_conv_format(ext)) >= 0) {
		struct conv_format *fmt = &conv_formats[i];
		struct string *conv_name = append_extension(name, fmt->dst);
		checked_fwrite(conv_name->text, conv_name->size, f);
		checked_fwrite(",", 1, f);
		checked_fwrite(ext, strlen(ext), f);
		// warn if the reverse conversion is not supported
		if (!fmt->rev_conv_supported && !fmt->warned) {
			WARNING("Conversion to .%s is not yet implemented", fmt->src);
			fmt->warned = true;
		}
		free_string(conv_name);
	} else {
		checked_fwrite(name, len, f);

	}
	checked_fwrite("\n", 1, f);
	free(name);
}

int command_ar_extract(int argc, char *argv[])
{
	char *output_file = NULL;
	char *file_name = NULL;
	char *manifest = NULL;
	int file_index = -1;

	uint32_t flags = 0;

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
			flags |= AR_FORCE;
			break;
		case LOPT_IMAGE_FORMAT:
			if (!strcasecmp(optarg, "png"))
				flags |= AR_IMGENC_BITS(ALCG_PNG);
			else if (!strcasecmp(optarg, "webp"))
				flags |= AR_IMGENC_BITS(ALCG_WEBP);
			else
				ERROR("Unsupported image format: \"%s\"", optarg);
			break;
		case LOPT_IMAGES_ONLY:
			flags |= AR_IMAGES_ONLY;
			break;
		case LOPT_RAW:
			raw = true;
			flags |= AR_RAW;
			break;
		case LOPT_MANIFEST:
			manifest = optarg;
			break;
		case LOPT_FLAT_PNG:
			flags |= AR_FLAT_PNG;
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
		ar_extract_index(ar, file_index, output_file, flags);
	} else if (file_name) {
		ar_extract_file(ar, file_name, output_file, flags);
	} else {
		ar_extract_all(ar, output_file, flags);
	}

	if (manifest) {
		FILE *f = checked_fopen(manifest, "wb");
		if (output_file) {
			checked_fwrite("#ALICEPACK --src-dir=", 21, f);
			checked_fwrite(output_file, strlen(output_file), f);
			checked_fwrite("\n", 1, f);
		} else {
			checked_fwrite("#ALICEPACK\n", 11, f);
		}
		checked_fwrite(argv[0], strlen(argv[0]), f);
		checked_fwrite("\n", 1, f);
		archive_for_each(ar, write_manifest_iter, f);
		fclose(f);
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
		{ "output",       'o', "Specify output file/directory",        required_argument, LOPT_OUTPUT },
		{ "index",        'i', "Specify file index",                   required_argument, LOPT_INDEX },
		{ "name",         'n', "Specify file name",                    required_argument, LOPT_NAME },
		{ "force",        'f', "Allow overwriting existing files",     no_argument,       LOPT_FORCE },
		{ "image-format", 0,   "Image output format (png or webp)",    required_argument, LOPT_IMAGE_FORMAT },
		{ "images-only",  0,   "Only extract images",                  no_argument,       LOPT_IMAGES_ONLY },
		{ "raw",          0,   "Don't convert image files",            no_argument,       LOPT_RAW },
		{ "manifest",     0,   "Write ALICEPACK manifest",             required_argument, LOPT_MANIFEST },
		{ "flat-png",     0,   "Extract images in .flat files as png", no_argument, LOPT_FLAT_PNG },
		{ 0 }
	}
};
