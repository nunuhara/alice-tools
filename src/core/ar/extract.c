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
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "alice.h"
#include "alice/ar.h"
#include "alice/ex.h"
#include "alice/flat.h"
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

static bool is_image_file(uint8_t *data)
{
	return cg_check_format(data) != ALCG_UNKNOWN;
}

static bool is_ex_file(uint8_t *data)
{
	return !memcmp(data, "HEAD", 4);
}

static bool is_flat_file(uint8_t *data)
{
	if (!memcmp(data, "ELNA", 4))
		return !memcmp(data+8, "FLAT", 4);
	return !memcmp(data, "FLAT", 4);
}

static enum filetype get_filetype(uint8_t *data, size_t size)
{
	if (size < 4)
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
static char *get_default_filename(const char *name, enum filetype ft, uint32_t flags)
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

	char *u = conv_output(name);

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
	bool is_image = is_image_file(data->data);
	bool output_img = !(flags & AR_RAW) && is_image;
	bool output_ex = !(flags & AR_RAW) && is_ex_file(data->data);

	if ((flags & AR_IMAGES_ONLY) && !is_image)
		return true;

	if (!output_file) {
		char *u = get_default_filename(data->name, ft, flags);
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

struct extract_all_iter_data {
	char *prefix;
	uint32_t flags;
	unsigned count;
	unsigned total;
};

static void _extract_all_iter(struct archive_data *data, struct extract_all_iter_data *iter_data);

static void extract_flat_image(uint8_t *data, size_t size, const char *section, unsigned i,
		struct string *prefix, struct extract_all_iter_data *iter_data)
{
	const char *ext = cg_file_extension(cg_check_format(data));
	char name[64];
	snprintf(name, 64, "%s.%u.%s", section, i, ext);

	struct archive_data dfile = {
		.name = name,
		.data = data,
		.size = size,
	};
	_extract_all_iter(&dfile, iter_data);
}

static void extract_flat_images(struct flat *flat, struct extract_all_iter_data *iter_data)
{
	struct string *prefix = string_conv_input(iter_data->prefix, strlen(iter_data->prefix));


	for (unsigned i = 0; i < flat->nr_libraries; i++) {
		struct flat_library *lib = &flat->libraries[i];
		if (lib->type != FLAT_LIB_CG)
			continue;
		extract_flat_image((uint8_t*)lib->cg.data, lib->cg.size, "libl", i, prefix, iter_data);
	}

	for (unsigned i = 0; i < flat->nr_talt_entries; i++) {
		struct talt_entry *e = &flat->talt_entries[i];
		extract_flat_image(flat->data + e->off, e->size, "talt", i, prefix, iter_data);
	}

	free_string(prefix);
}

static void display_filename(const char *name, struct extract_all_iter_data *iter_data)
{
	if (iter_data->flags & AR_PROGRESS) {
		unsigned progress = ((float)iter_data->count / (float)iter_data->total) * 100;
		NOTICE("%u", progress);
		NOTICE("# %s", name);
	} else {
		NOTICE("%s", name);
	}
}

static void extract_flat(struct archive_data *data, struct extract_all_iter_data *iter_data)
{
	int error;
	struct flat *flat = flat_open(data->data, data->size, &error);
	if (!flat) {
		WARNING("Error opening FLAT archive: %s", archive_strerror(error));
		return;
	}

	struct string *outfile = make_string(iter_data->prefix, strlen(iter_data->prefix));
	struct string *uname = string_conv_output(data->name, strlen(data->name));
	string_append(&outfile, uname);

	if (iter_data->flags & AR_IMAGES_ONLY) {
		NOTICE("Extracting %s...", uname->text);
		string_push_back(&outfile, '.');
		struct extract_all_iter_data flat_iter_data = {
			.prefix = outfile->text,
			.flags = iter_data->flags,
			.count = iter_data->count,
			.total = iter_data->total,
		};
		extract_flat_images(flat, &flat_iter_data);
	} else {
		display_filename(outfile->text, iter_data);
		mkdir_for_file(outfile->text);
		string_append_cstr(&outfile, ".x", 2);
		flat_extract(flat, outfile->text, iter_data->flags & AR_FLAT_PNG);
	}

	flat_free(flat);
	free_string(outfile);
	free_string(uname);
}

static void _extract_all_iter(struct archive_data *data, struct extract_all_iter_data *iter_data)
{
	enum filetype ft = get_filetype(data->data, data->size);

	if (!(iter_data->flags & AR_RAW) && ft == FT_FLAT) {
		extract_flat(data, iter_data);
		return;
	}

	bool is_image = is_image_file(data->data);
	char *file_name = get_default_filename(data->name, ft, iter_data->flags);
	char output_file[PATH_MAX];
	snprintf(output_file, PATH_MAX, "%s%s", iter_data->prefix, file_name);
	free(file_name);
	if (!is_image && (iter_data->flags & AR_IMAGES_ONLY)) {
		NOTICE("Skipping non-image file: %s", output_file);
		return;
	}

	mkdir_for_file(output_file);

	if (write_file(data, output_file, ft, iter_data->flags))
		display_filename(output_file, iter_data);
	else
		NOTICE("Skipping existing file: %s", output_file);
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

	_extract_all_iter(data, iter_data);
	iter_data->count++;
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
	struct extract_all_iter_data data = {
		.prefix = output_file,
		.flags = flags,
		.count = 1,
		.total = archive_nr_files(ar),
	};
	if (data.total == 0)
		data.total = 1;
	archive_for_each(ar, extract_all_iter, &data);
	if (flags & AR_PROGRESS)
		NOTICE("# Finished extracting to %s", output_file);
	free(output_file);
}

void ar_extract_file(struct archive *ar, char *file_name, char *output_file, uint32_t flags)
{
	check_flags(&flags);
	char *u = utf2sjis(file_name, strlen(file_name));
	struct archive_data *d = archive_get_by_name(ar, u);
	if (!d)
		ALICE_ERROR("No file with name \"%s\"", u);
	write_file(d, output_file, get_filetype(d->data, d->size), flags);
	archive_free_data(d);
	free(u);
}

void ar_extract_index(struct archive *ar, int file_index, char *output_file, uint32_t flags)
{
	check_flags(&flags);
	struct archive_data *d = archive_get(ar, file_index);
	if (!d)
		ALICE_ERROR("No file with index %d", file_index);
	write_file(d, output_file, get_filetype(d->data, d->size), flags);
	archive_free_data(d);
}
