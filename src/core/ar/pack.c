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
#include <sys/stat.h>
#include <unistd.h>
#include "system4.h"
#include "system4/archive.h"
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/flat.h"
#include "system4/string.h"
#include "system4/cg.h"
#include "alice.h"
#include "alice/ar.h"
#include "alice/ex.h"
#include "alice/flat.h"

static char path_separator = '/';

void ar_set_path_separator(char c)
{
	path_separator = c;
}

const char * const ar_ft_extensions[] = {
	[AR_FT_UNKNOWN] = "dat",
	[AR_FT_PNG] = "png",
	[AR_FT_QNT] = "qnt",
	[AR_FT_X] = "x",
	[AR_FT_TXTEX] = "txtex",
	[AR_FT_EX] = "ex",
	[AR_FT_PACTEX] = "pactex",
	[AR_FT_FLAT] = "flat",
};

static enum ar_filetype cg_type_to_ar_filetype(enum cg_type type)
{
	switch (type) {
	case ALCG_PNG: return AR_FT_PNG;
	case ALCG_QNT: return AR_FT_QNT;
	default: ALICE_ERROR("Unsupported CG type");
	}
}

static enum cg_type ar_filetype_to_cg_type(enum ar_filetype type)
{
	switch (type) {
	case AR_FT_PNG: return ALCG_PNG;
	case AR_FT_QNT: return ALCG_QNT;
	default: ALICE_ERROR("Unsupported CG type");
	}
}

static struct ar_file_spec **alicepack_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	struct ar_file_spec **files = xcalloc(mf->nr_rows, sizeof(struct ar_file_spec*));
	*size_out = mf->nr_rows;

	for (size_t i = 0; i < mf->nr_rows; i++) {
		files[i] = xmalloc(sizeof(struct ar_file_spec));
		files[i]->type = AR_FILE_SPEC_DISK;
		files[i]->disk.path = string_ref(mf->alicepack[i].filename);
		files[i]->name = string_dup(mf->alicepack[i].filename);
	}

	return files;
}

static void convert_file(struct string *src, enum ar_filetype src_fmt, struct string *dst, enum ar_filetype dst_fmt)
{
	// ensure directory exists for dst
	mkdir_for_file(dst->text);

	switch (src_fmt) {
	case AR_FT_PNG:
	case AR_FT_QNT: {
		struct cg *cg = cg_load_file(src->text);
		if (!cg)
			return;
		FILE *f = checked_fopen(dst->text, "wb");
		if (!cg_write(cg, ar_filetype_to_cg_type(dst_fmt), f)) {
			ALICE_ERROR("failed to encode file \"%s\"", dst->text);
		}
		fclose(f);
		cg_free(cg);
		break;
	}
	case AR_FT_X:
	case AR_FT_TXTEX: {
		if (dst_fmt != AR_FT_EX && dst_fmt != AR_FT_PACTEX)
			ALICE_ERROR("Invalid output format for .txtex files");
		struct ex *ex = ex_parse_file(src->text);
		if (!ex)
			ALICE_ERROR("Failed to parse .txtex file: %s", src->text);
		ex_write_file(dst->text, ex);
		ex_free(ex);
		break;
	}
	default:
		ALICE_ERROR("Filetype not supported as source format");
	}
}

/*
 * Since .flat files are somewhat of an archive-type of their own, some special
 * handling is required compared to other file types.
 */
static void convert_flat(struct string *src, enum ar_filetype src_fmt,
			 struct string *dst_dir, const char *name)
{
	if (src_fmt != AR_FT_X && src_fmt != AR_FT_TXTEX)
		ALICE_ERROR("Invalid input format for .flat conversion");

	// XXX: we only show the "wrong file extension" message for
	//      unexpected extensions (to reduce console spam)
	const char *ext = file_extension(name);
	if (strcasecmp(ext, ar_ft_extensions[src_fmt])) {
		if (!strcasecmp(ext, "z"))
			return;
		if (!strcasecmp(ext, "head"))
			return;
		if (!strcasecmp(ext, "mtlc"))
			return;
		if (!strcasecmp(ext, "tmnl"))
			return;
		if (!strcasecmp(ext, "ajp"))
			return;
		if (!strcasecmp(ext, "png"))
			return;
		if (!strcasecmp(ext, "qnt"))
			return;
		struct string *dst = string_path_join(dst_dir, name);
		NOTICE("Skipping \"%s\": wrong file extension", dst->text);
		free_string(dst);
		return;
	}

	// TODO: Only rebuild .flat files if the inputs have been modified.
	//       This will require splitting flat_build into two parts:
	//         * flat_read_manifest -> manifest object
	//         * flat_load_manifest -> flat object
	//       In between, we can stat the input files to check timestamps.
	struct string *output_path = NULL;
	struct flat_archive *flat = flat_build(src->text, &output_path);
	if (output_path) {
		struct string *tmp = string_path_join(dst_dir, output_path->text);
		free_string(output_path);
		output_path = tmp;
	} else {
		struct string *tmp = string_path_join(dst_dir, name);
		output_path = replace_extension(tmp->text, "flat");
		free_string(tmp);
	}

	mkdir_for_file(output_path->text);
	FILE *out = checked_fopen(output_path->text, "wb");
	checked_fwrite(flat->data, flat->data_size, out);
	fclose(out);

	archive_free(&flat->ar);
	free_string(output_path);
}

static void convert_dir(struct string *src_dir, enum ar_filetype src_fmt,
			struct string *dst_dir, enum ar_filetype dst_fmt)
{
	char *d_name;
	UDIR *d = checked_opendir(src_dir->text);
	while ((d_name = readdir_utf8(d)) != NULL) {
		if (d_name[0] == '.') {
			free(d_name);
			continue;
		}

		ustat src_s;
		struct string *src_path = string_path_join(src_dir, d_name);
		struct string *dst_base = string_path_join(dst_dir, d_name);
		struct string *dst_path = replace_extension(dst_base->text, ar_ft_extensions[dst_fmt]);
		checked_stat(src_path->text, &src_s);

		if (S_ISDIR(src_s.st_mode)) {
			convert_dir(src_path, src_fmt, dst_base, dst_fmt);
			goto loop_next;
		}
		if (!S_ISREG(src_s.st_mode)) {
			NOTICE("Skipping \"%s\": not a regular file", src_path->text);
			goto loop_next;
		}
		// flat conversion is a special case
		if (dst_fmt == AR_FT_FLAT) {
			convert_flat(src_path, src_fmt, dst_dir, d_name);
			goto loop_next;
		}

		if (strcasecmp(file_extension(d_name), ar_ft_extensions[src_fmt])) {
			NOTICE("Skipping \"%s\": wrong file extension", src_path->text);
			goto loop_next;
		}

		if (file_exists(dst_path->text)) {
			ustat dst_s;
			checked_stat(dst_path->text, &dst_s);
			if (src_s.st_mtime < dst_s.st_mtime)
				goto loop_next;
		}

		NOTICE("%s -> %s", src_path->text, dst_path->text);

		// skip transcode if src/dst formats match
		if (src_fmt == dst_fmt) {
			if (!file_copy(src_path->text, dst_path->text)) {
				ALICE_ERROR("failed to copy file \"%s\": %s", dst_path->text, strerror(errno));
			}
			goto loop_next;
		}

		convert_file(src_path, src_fmt, dst_path, dst_fmt);

	loop_next:
		free_string(src_path);
		free_string(dst_path);
		free_string(dst_base);
		free(d_name);
	}
	closedir_utf8(d);
}

static void batchpack_convert(struct batchpack_line *line)
{
	convert_dir(line->src, line->src_fmt, line->dst, line->dst_fmt);
}

static void dir_to_file_list(struct string *dst, struct string *base_name, ar_file_list *files, enum ar_filetype fmt)
{
	// add all files in dst to file list
	// TODO: filter by file extension?
	char *d_name;
	UDIR *d = checked_opendir(dst->text);
	while ((d_name = readdir_utf8(d)) != NULL) {
		if (d_name[0] == '.')
			goto loop_next;

		ustat s;
		struct string *path = string_path_join(dst, d_name);
		struct string *name = string_path_join(base_name, d_name);
		checked_stat(path->text, &s);

		if (S_ISDIR(s.st_mode)) {
			dir_to_file_list(path, name, files, fmt);
			free_string(path);
			free_string(name);
			goto loop_next;
		}
		if (!S_ISREG(s.st_mode)) {
			WARNING("Skipping \"%s\": not a regular file", path->text);
			free_string(path);
			free_string(name);
			goto loop_next;
		}
		if (fmt && strcasecmp(file_extension(d_name), ar_ft_extensions[fmt])) {
			NOTICE("Skipping \"%s\": wrong file extension", path->text);
			free_string(path);
			free_string(name);
			goto loop_next;
		}

		struct ar_file_spec *spec = xmalloc(sizeof(struct ar_file_spec));
		spec->type = AR_FILE_SPEC_DISK;
		spec->disk.path = path;
		spec->name = name;
		kv_push(struct ar_file_spec*, *files, spec);
loop_next:
		free(d_name);
	}
	closedir_utf8(d);
}

void ar_dir_to_file_list(struct string *dir, ar_file_list *files, enum ar_filetype fmt)
{
	dir_to_file_list(dir, &EMPTY_STRING, files, fmt);
}

static int file_spec_compare(const void *_a, const void *_b)
{
	const struct ar_file_spec *a = *((const struct ar_file_spec**)_a);
	const struct ar_file_spec *b = *((const struct ar_file_spec**)_b);
	return strcmp(a->name->text, b->name->text);
}

void ar_file_list_sort(ar_file_list *list)
{
	qsort(list->a, list->n, sizeof(struct ar_file_spec*), file_spec_compare);
}

static struct ar_file_spec **batchpack_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	ar_file_list files;
	kv_init(files);

	// convert files
	for (size_t i = 0; i < mf->nr_rows; i++) {
		struct string *src = mf->batchpack[i].src;
		struct string *dst = mf->batchpack[i].dst;

		if (!is_directory(dst->text))
			ALICE_ERROR("line %d: \"%s\" is not a directory", (int)i+2, dst->text);

		// don't convert if src and dst directories are the same
		if (strcmp(src->text, dst->text)) {
			if (!is_directory(src->text))
				ALICE_ERROR("line %d: \"%s\" is not a directory", (int)i+2, src->text);
			batchpack_convert(mf->batchpack+i);
		}
	}

	// create file list from output dirs
	for (size_t i = 0; i < mf->nr_rows; i++) {
		struct string *dst = mf->batchpack[i].dst;
		// prevent double-packing the same output directory
		bool duplicate = false;
		for (int j = 0; j < i; j++) {
			if (!strcmp(dst->text, mf->batchpack[j].dst->text)) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) {
			dir_to_file_list(dst, string_ref(&EMPTY_STRING), &files, mf->batchpack[i].dst_fmt);
		}
	}

	ar_file_list_sort(&files);

	*size_out = files.n;
	return files.a;
}

static void alicecg2_to_batchpack(struct ar_manifest *mf)
{
	struct batchpack_line *lines = xcalloc(mf->nr_rows, sizeof(struct batchpack_line));
	for (size_t i = 0; i < mf->nr_rows; i++) {
		lines[i].src = mf->alicecg2[i].src;
		lines[i].dst = mf->alicecg2[i].dst;
		lines[i].src_fmt = cg_type_to_ar_filetype(mf->alicecg2[i].src_fmt);
		lines[i].dst_fmt = cg_type_to_ar_filetype(mf->alicecg2[i].dst_fmt);
	}
	free(mf->alicecg2);
	mf->batchpack = lines;
	mf->type = AR_MF_BATCHPACK;
}

static void ar_to_file_spec_iter(struct archive_data *data, void *user)
{
	ar_file_list *files = user;
	if (!archive_load_file(data))
		ALICE_ERROR("Error loading archive file: %s", data->name);

	struct ar_file_spec *spec = xmalloc(sizeof(struct ar_file_spec));
	spec->type = AR_FILE_SPEC_MEM;
	spec->mem.data = xmalloc(data->size);
	spec->mem.size = data->size;
	memcpy(spec->mem.data, data->data, data->size);

	char *tmp = conv_input(data->name);
	spec->name = cstr_to_string(tmp);
	free(tmp);

	kv_push(struct ar_file_spec*, *files, spec);
}

void ar_to_file_list(struct archive *ar, ar_file_list *files)
{
	archive_for_each(ar, ar_to_file_spec_iter, files);
}

static struct ar_file_spec **manifest_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	struct ar_file_spec **files;
	switch (mf->type) {
	case AR_MF_ALICEPACK:
		files = alicepack_to_file_list(mf, size_out);
		break;
	case AR_MF_ALICECG2:
		alicecg2_to_batchpack(mf);
		files = batchpack_to_file_list(mf, size_out);
		break;
	case AR_MF_BATCHPACK:
		files = batchpack_to_file_list(mf, size_out);
		break;
	case AR_MF_NL5:
	case AR_MF_WAVLINKER:
	case AR_MF_INVALID:
	default:
		ALICE_ERROR("Invalid manifest type");
		break;
	}

	// handle file separator in file names
	for (size_t i = 0; i < *size_out; i++) {
		for (int j = 0; files[i]->name->text[j]; j++) {
			char c = files[i]->name->text[j];
			if (c == '/' || c == '\\') {
				files[i]->name->text[j] = path_separator;
			}
		}
	}

	return files;
}

static void free_manifest(struct ar_manifest *mf)
{
	switch (mf->type) {
	case AR_MF_ALICEPACK:
		for (size_t i = 0; i < mf->nr_rows; i++) {
			free_string(mf->alicepack[i].filename);
		}
		free(mf->alicepack);
		break;
	case AR_MF_ALICECG2:
		for (size_t i = 0; i < mf->nr_rows; i++) {
			free_string(mf->alicecg2[i].src);
			free_string(mf->alicecg2[i].dst);
		}
		free(mf->alicecg2);
		break;
	case AR_MF_BATCHPACK:
		for (size_t i = 0; i < mf->nr_rows; i++) {
			free_string(mf->batchpack[i].src);
			free_string(mf->batchpack[i].dst);
		}
		free(mf->batchpack);
		break;
	case AR_MF_NL5:
	case AR_MF_WAVLINKER:
	case AR_MF_INVALID:
	default:
		ALICE_ERROR("Invalid manifest type");
	}
	free_string(mf->output_path);
	free(mf);
}

void ar_file_spec_free(struct ar_file_spec *spec)
{
	free_string(spec->name);
	switch (spec->type) {
	case AR_FILE_SPEC_DISK:
		free_string(spec->disk.path);
		break;
	case AR_FILE_SPEC_MEM:
		free(spec->mem.data);
		break;
	}
	free(spec);
}

void ar_file_list_free(ar_file_list *list)
{
	for (unsigned i = 0; i < list->n; i++) {
		ar_file_spec_free(list->a[i]);
	}
}

void ar_pack_manifest(struct ar_manifest *ar, int afa_version)
{
	size_t nr_files;
	struct ar_file_spec **files = manifest_to_file_list(ar, &nr_files);
	write_afa(ar->output_path, files, nr_files, afa_version);
	for (size_t i = 0; i < nr_files; i++) {
		ar_file_spec_free(files[i]);
	}
	free(files);
}

void ar_pack(const char *manifest, int afa_version)
{
	struct ar_manifest *mf = ar_parse_manifest(manifest);
	if (mf->afa_version > 0)
		afa_version = mf->afa_version;
	if (mf->backslash)
		ar_set_path_separator('\\');

	const char *ext = file_extension(mf->output_path->text);
	if (!ext || strcasecmp(ext, "afa"))
		ALICE_ERROR("Only .afa archives supported");

	// paths are relative to manifest location
	char *old_cwd = xmalloc(2048);
	old_cwd = getcwd(old_cwd, 2048);
	chdir_to_file(manifest);

	ar_pack_manifest(mf, afa_version);
	free_manifest(mf);
	chdir(old_cwd);
	free(old_cwd);
}
