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

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "system4.h"
#include "system4/archive.h"
#include "system4/ex.h"
#include "system4/flat.h"
#include "system4/utfsjis.h"
#include "alice.h"
#include "alice/ar.h"
#include "alice/ex.h"
#include "alice/port.h"

enum filetype {
	FT_UNKNOWN,
	FT_IMAGE,
	FT_EX,
	FT_FLAT,
};

// Add a trailing slash to a path
static char *output_file_dir(const char *path)
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
static char *get_default_filename(struct archive_data *data, enum filetype ft, uint32_t flags)
{
	// get conversion extension
	const char *ext = NULL;
	if (!(flags & AR_RAW)) {
		if (ft == FT_IMAGE) {
			ext = cg_file_extensions[AR_IMGENC(flags)];
		} else if (ft == FT_EX) {
			ext = "x";
		}
	}

	char *u = conv_output(data->name);

	if (ext) {
		size_t ulen = strlen(u);
		size_t extlen = strlen(ext);
		u = xrealloc(u, ulen + extlen + 2);
		u[ulen] = '.';
		memcpy(u+ulen+1, ext, extlen + 1);
	}

	for (int i = 0; u[i]; i++) {
		if (u[i] == '\\')
			u[i] = '/';
	}
	return u;
}

/*
 * Write an archived file to disk.
 */
static bool write_file(struct archive_data *data, const char *output_file, enum filetype ft, uint32_t flags)
{
	FILE *f = NULL;
	bool is_image = is_image_file(data);
	bool output_img = !(flags & AR_RAW) && is_image;
	bool output_ex = !(flags & AR_RAW) && is_ex_file(data);

	if ((flags & AR_IMAGES_ONLY) && !is_image)
		return true;

	if (!output_file) {
		char *u = get_default_filename(data, ft, flags);
		mkdir_for_file(u);
		if (!(flags & AR_FORCE) && file_exists(u)) {
			free(u);
			return false;
		}
		if (!(f = checked_fopen(u, "wb"))) {
			free(u);
			return false;
		}
		free(u);
	} else if (strcmp(output_file, "-")) {
		if (!(flags & AR_FORCE) && file_exists(output_file)) {
			return false;
		}
		if (!(f = checked_fopen(output_file, "wb"))) {
			return false;
		}
	}

	if (output_img) {
		struct cg *cg = cg_load_data(data);
		if (cg) {
			cg_write(cg, AR_IMGENC(flags), f);
			cg_free(cg);
		} else {
			WARNING("Failed to load CG");
		}
	} else if (output_ex) {
		struct ex *ex = ex_read(data->data, data->size);
		if (ex) {
			struct port port;
			port_file_init(&port, f);
			ex_dump(&port, ex);
			ex_free(ex);
			port_close(&port);
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

struct extract_all_iter_data {
	char *prefix;
	uint32_t flags;
};

static void extract_flat(struct archive_data *data, char *output_dir, uint32_t flags)
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

	struct extract_all_iter_data iter_data = { .prefix = prefix, .flags = flags };
	archive_for_each(ar, extract_all_iter, &iter_data);
	archive_free(ar);
	free(prefix);
	free(uname);
}

static void extract_all_iter(struct archive_data *data, void *_iter_data)
{
	struct extract_all_iter_data *iter_data = _iter_data;

	if (!archive_load_file(data)) {
		char *u = conv_output(data->name);
		WARNING("Error loading file: %s", u);
		free(u);
		return;
	}

	enum filetype ft = get_filetype(data);

	if (!(iter_data->flags & AR_RAW) && ft == FT_FLAT) {
		char *u = conv_output(data->name);
		NOTICE("Extracting %s...", u);
		free(u);
		extract_flat(data, iter_data->prefix, iter_data->flags);
		return;
	}

	bool is_image = is_image_file(data);
	char *file_name = get_default_filename(data, ft, iter_data->flags);
	char output_file[PATH_MAX];
	snprintf(output_file, PATH_MAX, "%s%s", iter_data->prefix, file_name);
	free(file_name);
	if (!is_image && (iter_data->flags & AR_IMAGES_ONLY)) {
		NOTICE("Skipping non-image file: %s", output_file);
		return;
	}

	mkdir_for_file(output_file);

	if (write_file(data, output_file, ft, iter_data->flags))
		NOTICE("%s", output_file);
	else
		NOTICE("Skipping existing file: %s", output_file);
}

static void check_flags(uint32_t *flags)
{
	if (AR_IMGENC(*flags) == 0) {
		*flags |= AR_IMGENC_BITS(ALCG_PNG);
	}
}

void ar_extract_all(struct archive *ar, const char *_output_file, uint32_t flags)
{
	check_flags(&flags);
	char *output_file = output_file_dir(_output_file);
	struct extract_all_iter_data data = { .prefix = output_file, .flags = flags };
	archive_for_each(ar, extract_all_iter, &data);
	free(output_file);
}

void ar_extract_file(struct archive *ar, char *file_name, char *output_file, uint32_t flags)
{
	check_flags(&flags);
	char *u = utf2sjis(file_name, strlen(file_name));
	struct archive_data *d = archive_get_by_name(ar, u);
	if (!d)
		ALICE_ERROR("No file with name \"%s\"", u);
	write_file(d, output_file, get_filetype(d), flags);
	archive_free_data(d);
	free(u);
}

void ar_extract_index(struct archive *ar, int file_index, char *output_file, uint32_t flags)
{
	check_flags(&flags);
	struct archive_data *d = archive_get(ar, file_index);
	if (!d)
		ALICE_ERROR("No file with index %d", file_index);
	write_file(d, output_file, get_filetype(d), flags);
	archive_free_data(d);
}
