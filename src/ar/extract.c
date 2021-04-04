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
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/flat.h"
#include "system4/png.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "system4/webp.h"
#include "alice.h"
#include "archive.h"

int ex_dump(FILE *out, struct ex *ex);

enum {
	LOPT_HELP = 256,
	LOPT_OUTPUT,
	LOPT_INDEX,
	LOPT_NAME,
	LOPT_FORCE,
	LOPT_IMAGE_FORMAT,
	LOPT_IMAGES_ONLY,
	LOPT_RAW,
	LOPT_TOC,
};

enum filetype {
	FT_UNKNOWN,
	FT_IMAGE,
	FT_EX,
	FT_FLAT,
};

static bool raw = false;
static bool force = false;
static bool images_only = false;

char **toc = NULL;
size_t toc_size = 0;

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

static enum cg_type imgenc = ALCG_PNG;

static bool is_image_file(struct archive_data *data)
{
	return cg_check_format(data->data) != ALCG_UNKNOWN;
}

static bool is_ex_file(struct archive_data *data)
{
	return !memcmp(data->data, "HEAD", 4);
}

static bool is_flat_file(struct archive_data *data)
{
	if (!memcmp(data->data, "ELNA", 4))
		return !memcmp(data->data+8, "FLAT", 4);
	return !memcmp(data->data, "FLAT", 4);
}

static enum filetype get_filetype(struct archive_data *data)
{
	if (data->size < 4)
		return FT_UNKNOWN;
	if (is_image_file(data))
		return FT_IMAGE;
	if (is_ex_file(data))
		return FT_EX;
	if (is_flat_file(data))
		return FT_FLAT;
	return FT_UNKNOWN;
}

/*
 * Determine the default filename to use for an archived file.
 */
static char *get_default_filename(struct archive_data *data, enum filetype ft)
{
	// get conversion extension
	const char *ext = NULL;
	if (!raw) {
		if (ft == FT_IMAGE) {
			ext = cg_file_extensions[imgenc];
		} else if (ft == FT_EX) {
			ext = "x";
		}
	}

	// get filename
	char *u;
	if (toc) {
		if ((size_t)data->no >= toc_size) {
			WARNING("index exceeds table-of-contents length");
			u = conv_output(data->name);
		} else {
			u = strdup(toc[data->no]);
		}
	} else {
		u = conv_output(data->name);
	}

	if (ext) {
		size_t ulen = strlen(u);
		size_t extlen = strlen(ext);
		u = xrealloc(u, ulen + extlen + 2);
		u[ulen] = '.';
		memcpy(u+ulen+1, ext, extlen + 1);
	}
	return u;
}

/*
 * Write an archived file to disk.
 */
static bool write_file(struct archive_data *data, const char *output_file, enum filetype ft)
{
	FILE *f = NULL;
	bool is_image = is_image_file(data);
	bool output_img = !raw && is_image;
	bool output_ex = !raw && is_ex_file(data);

	if (images_only && !is_image)
		return true;

	if (!output_file) {
		char *u = get_default_filename(data, ft);
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
			cg_write(cg, imgenc, f);
			cg_free(cg);
		} else {
			WARNING("Failed to load CG");
		}
	} else if (output_ex) {
		struct ex *ex = ex_read(data->data, data->size);
		if (ex) {
			ex_dump(f, ex);
			ex_free(ex);
		} else {
			WARNING("Failed to load .ex file");
		}
	} else if (fwrite(data->data, data->size, 1, f ? f : stdout) != 1) {
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
	struct archive *ar = (struct archive*)flat_open(data->data, data->size, &error);
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

	archive_for_each(ar, extract_all_iter, prefix);
	archive_free(ar);
	free(prefix);
	free(uname);
}

static void extract_all_iter(struct archive_data *data, void *_prefix)
{
	if (!archive_load_file(data)) {
		char *u = conv_output(data->name);
		WARNING("Error loading file: %s", u);
		free(u);
		return;
	}

	enum filetype ft = get_filetype(data);

	if (!raw && ft == FT_FLAT) {
		char *u = conv_output(data->name);
		NOTICE("Extracting %s...", u);
		free(u);
		extract_flat(data, _prefix);
		return;
	}

	char *prefix = _prefix;
	bool is_image = is_image_file(data);
	char *file_name = get_default_filename(data, ft);
	char output_file[PATH_MAX];
	snprintf(output_file, PATH_MAX, "%s%s", prefix, file_name);
	free(file_name);
	if (!is_image && images_only) {
		NOTICE("Skipping non-image file: %s", output_file);
		return;
	}

	mkdir_for_file(output_file);

	if (write_file(data, output_file, ft))
		NOTICE("%s", output_file);
	else
		NOTICE("Skipping existing file: %s", output_file);
}

static char *toc_read_filename(char **data)
{
	char *p = *data;
	if (!p)
		return NULL;

	// skip initial ws
	while (*p == '\n' || *p == '\r') {
		p++;
	}

	// skip filename
	char *start = p;
	while (*p && *p != '\r' && *p != '\n') {
		p++;
	}

	// write null terminator
	if (*p == 0) {
		*data = NULL;
	} else {
		*p = 0;
		*data = p+1;
	}

	return start;
}

static char **parse_toc(const char *toc_file, size_t *_nr_files)
{
	size_t file_size;
	char *file_data = file_read(toc_file, &file_size);

	char *filename;
	char *p = file_data;
	char **files = NULL;
	size_t nr_files = 0;
	while ((filename = toc_read_filename(&p))) {
		files = xrealloc(files, (nr_files+1) * sizeof(char*));
		files[nr_files++] = strdup(filename);
	}

	free(file_data);
	*_nr_files = nr_files;
	return files;
}

int command_ar_extract(int argc, char *argv[])
{
	char *output_file = NULL;
	char *toc_file = NULL;
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
				imgenc = ALCG_PNG;
			else if (!strcasecmp(optarg, "webp"))
				imgenc = ALCG_WEBP;
			else
				ERROR("Unrecognized image format: \"%s\"", optarg);
			break;
		case LOPT_IMAGES_ONLY:
			images_only = true;
			break;
		case LOPT_RAW:
			raw = true;
			break;
		case LOPT_TOC:
			toc_file = optarg;
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

	if (toc_file) {
		toc = parse_toc(toc_file, &toc_size);
	}

	// run command
	if (file_index >= 0) {
		struct archive_data *d = archive_get(ar, file_index);
		if (!d)
			ERROR("No file with index %d", file_index);
		write_file(d, output_file, get_filetype(d));
		archive_free_data(d);
	} else if (file_name) {
		char *u = utf2sjis(file_name, strlen(file_name));
		struct archive_data *d = archive_get_by_name(ar, u);
		if (!d)
			ERROR("No file with name \"%s\"", u);
		write_file(d, output_file, get_filetype(d));
		archive_free_data(d);
		free(u);
	} else {
		output_file = output_file_dir(output_file);
		archive_for_each(ar, extract_all_iter, output_file);
		free(output_file);
	}

	archive_free(ar);
	for (size_t i = 0; i < toc_size; i++) {
		free(toc[i]);
	}
	free(toc);
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
		{ "toc",          0,   "Specify table of contents file",    required_argument, LOPT_TOC },
		{ 0 }
	}
};
