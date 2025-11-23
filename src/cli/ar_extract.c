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
#include "system4/dcf.h"
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

#define CACHE_DIRNAME "alice-tools-cache"

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
	LOPT_CACHE,
	LOPT_NO_CACHE,
	LOPT_FLAT_PNG,
	LOPT_PROGRESS,
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
	{ "dcf",    "png", true },
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

static void write_manifest_filename(const char *name, size_t len, bool need_quotes, FILE *f)
{
	if (need_quotes) {
		char *escaped = escape_string_noconv(name);
		checked_fwrite("\"", 1, f);
		checked_fwrite(escaped, strlen(escaped), f);
		checked_fwrite("\"", 1, f);
		free(escaped);
	} else {
		checked_fwrite(name, len, f);
	}
}

static void write_manifest_iter(struct archive_data *data, void *_f)
{
	FILE *f = _f;
	char *name = conv_output(data->name);
	bool need_quotes = false;
	size_t len;
	for (len = 0; name[len]; len++) {
		switch (name[len]) {
		case '\\':
			name[len] = '/';
			break;
		case ',':
		case ' ':
		case '\n':
		case '\t':
		case '\r':
		case '\b':
		case '\f':
			need_quotes = true;
			break;
		}
	}

	int i;
	const char *ext = file_extension(name);
	if (!raw && (i = is_conv_format(ext)) >= 0) {
		struct conv_format *fmt = &conv_formats[i];
		struct string *conv_name = replace_extension(name, fmt->dst);
		write_manifest_filename(conv_name->text, conv_name->size, need_quotes, f);
		checked_fwrite(",", 1, f);
		if (!strcasecmp(ext, "ajp")) {
			// XXX: round-trip from AJP->PNG->AJP will cause quality loss,
			//      so we encode as QNT instead
			checked_fwrite("qnt", 3, f);
		} else {
			checked_fwrite(ext, strlen(ext), f);
		}
		if (!strcasecmp(ext, "dcf")) {
			archive_load_file(data);
			char *base_sjis = dcf_get_base_cg_name(data->data, data->size);
			if (base_sjis) {
				char *tmp = conv_output(base_sjis);
				// XXX: the game doesn't care about the extension when looking
				//      up the base CG, so we can just use the converted PNG
				struct string *base_utf = replace_extension(tmp, "png");
				checked_fwrite(",", 1, f);
				checked_fwrite(base_utf->text, base_utf->size, f);
				free_string(base_utf);
				free(tmp);
				free(base_sjis);
			} else {
				WARNING("Unable to determine base CG for \"%s\"", conv_name->text);
			}
			archive_release_file(data);
		}
		// warn if the reverse conversion is not supported
		if (!fmt->rev_conv_supported && !fmt->warned) {
			WARNING("Conversion to .%s is not yet implemented", fmt->src);
			fmt->warned = true;
		}
		free_string(conv_name);
	} else {
		write_manifest_filename(name, len, need_quotes, f);
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
	bool no_cache = false;
	bool yes_cache = false;
	bool cache = false;

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
		case LOPT_CACHE:
			yes_cache = true;
			break;
		case LOPT_NO_CACHE:
			no_cache = true;
			break;
		case LOPT_FLAT_PNG:
			flags |= AR_FLAT_PNG;
			break;
		case LOPT_PROGRESS:
			flags |= AR_PROGRESS;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	// check argument count
	if (argc != 1) {
		USAGE_ERROR(&cmd_ar_extract, "Wrong number of arguments");
	}

	// XXX: enable cache by default for CG archives
	const char *sub = strstr(argv[0], "CG");
	if (sub && !raw) {
		if (sub[2] == '.' || (sub[2] && isdigit(sub[2]) && sub[3] == '.'))
			cache = true;
	}
	if (yes_cache)
		cache = true;
	if (no_cache)
		cache = false;

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

		if (manifest) {
			char *cache_dir = NULL;
			if (cache) {
				if (output_file)
					cache_dir = path_join(output_file, CACHE_DIRNAME);
				else
					cache_dir = xstrdup(CACHE_DIRNAME);
			}

			FILE *f = checked_fopen(manifest, "wb");
			checked_fwrite("#ALICEPACK", 10, f);
			if (output_file) {
				checked_fwrite(" --src-dir=", 11, f);
				checked_fwrite(output_file, strlen(output_file), f);
			}
			if (cache_dir) {
				checked_fwrite(" --cache-dir=", 13, f);
				checked_fwrite(cache_dir, strlen(cache_dir), f);
			}
			checked_fwrite("\n", 1, f);
			checked_fwrite(argv[0], strlen(argv[0]), f);
			checked_fwrite("\n", 1, f);
			archive_for_each(ar, write_manifest_iter, f);
			fclose(f);

			if (cache_dir) {
				ar_extract_all(ar, cache_dir, flags | AR_RAW);
				free(cache_dir);
			}
		}
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
		{ "cache",        0,   "Create cache for manifest",            no_argument,       LOPT_CACHE },
		{ "no-cache",     0,   "Don't create cache for manifest",      no_argument,       LOPT_NO_CACHE },
		{ "flat-png",     0,   "Extract images in .flat files as png", no_argument,       LOPT_FLAT_PNG },
		{ "progress",     0,   "Display extraction progress",          no_argument,       LOPT_PROGRESS },
		{ 0 }
	}
};
