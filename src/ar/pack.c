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
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
#include "system4.h"
#include "system4/archive.h"
#include "system4/cg.h"
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/flat.h"
#include "system4/string.h"
#include "alice.h"
#include "alice-ar.h"

void write_afa(struct string *filename, struct ar_file_spec **files, size_t nr_files);

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
		files[i]->path = string_ref(mf->alicepack[i].filename);
		files[i]->name = string_ref(mf->alicepack[i].filename);
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
		struct string *dst = path_join(dst_dir, name);
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
		struct string *tmp = path_join(dst_dir, output_path->text);
		free_string(output_path);
		output_path = tmp;
	} else {
		struct string *tmp = path_join(dst_dir, name);
		output_path = replace_extension(tmp->text, "flat");
		free_string(tmp);
	}

	FILE *out = checked_fopen(output_path->text, "wb");
	checked_fwrite(flat->data, flat->data_size, out);
	fclose(out);

	archive_free(&flat->ar);
	free_string(output_path);
}

static void convert_dir(struct string *src_dir, enum ar_filetype src_fmt,
			struct string *dst_dir, enum ar_filetype dst_fmt)
{
	struct dirent *dir;
	DIR *d = checked_opendir(src_dir->text);
	while ((dir = readdir(d)) != NULL) {
		if (dir->d_name[0] == '.')
			continue;

		struct stat src_s;
		struct string *src_path = path_join(src_dir, dir->d_name);
		struct string *dst_base = path_join(dst_dir, dir->d_name);
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
			convert_flat(src_path, src_fmt, dst_dir, dir->d_name);
			goto loop_next;
		}

		if (strcasecmp(file_extension(dir->d_name), ar_ft_extensions[src_fmt])) {
			NOTICE("Skipping \"%s\": wrong file extension", src_path->text);
			goto loop_next;
		}

		if (file_exists(dst_path->text)) {
			struct stat dst_s;
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
	}
	closedir(d);
}

static void batchpack_convert(struct batchpack_line *line)
{
	convert_dir(line->src, line->src_fmt, line->dst, line->dst_fmt);
}

kv_decl(filelist, struct ar_file_spec*);

static void dir_to_file_list(struct string *dst, struct string *base_name, filelist *files, enum ar_filetype fmt)
{
	// add all files in dst to file list
	// TODO: filter by file extension?
	struct dirent *dir;
	DIR *d = checked_opendir(dst->text);
	while ((dir = readdir(d)) != NULL) {
		if (dir->d_name[0] == '.')
			continue;

		struct stat s;
		struct string *path = path_join(dst, dir->d_name);
		struct string *name = path_join(base_name, dir->d_name);
		checked_stat(path->text, &s);

		if (S_ISDIR(s.st_mode)) {
			dir_to_file_list(path, name, files, fmt);
			free_string(path);
			free_string(name);
			continue;
		}
		if (!S_ISREG(s.st_mode)) {
			WARNING("Skipping \"%s\": not a regular file", path->text);
			free_string(path);
			free_string(name);
			continue;
		}
		if (fmt && strcasecmp(file_extension(dir->d_name), ar_ft_extensions[fmt])) {
			NOTICE("Skipping \"%s\": wrong file extension", path->text);
			free_string(path);
			free_string(name);
			continue;
		}

		struct ar_file_spec *spec = xmalloc(sizeof(struct ar_file_spec));
		spec->path = path;
		spec->name = name;
		kv_push(struct ar_file_spec*, *files, spec);
	}
	closedir(d);
}

static int file_spec_compare(const void *_a, const void *_b)
{
	const struct ar_file_spec *a = *((const struct ar_file_spec**)_a);
	const struct ar_file_spec *b = *((const struct ar_file_spec**)_b);
	return strcmp(a->name->text, b->name->text);
}

static struct ar_file_spec **batchpack_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	filelist files;
	kv_init(files);
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

		dir_to_file_list(dst, string_ref(&EMPTY_STRING), &files, mf->batchpack[i].dst_fmt);
	}

	qsort(files.a, files.n, sizeof(struct ar_file_spec*), file_spec_compare);

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

static struct ar_file_spec **manifest_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	switch (mf->type) {
	case AR_MF_ALICEPACK:
		return alicepack_to_file_list(mf, size_out);
	case AR_MF_ALICECG2:
		alicecg2_to_batchpack(mf);
		return batchpack_to_file_list(mf, size_out);
	case AR_MF_BATCHPACK:
		return batchpack_to_file_list(mf, size_out);
	case AR_MF_NL5:
	case AR_MF_WAVLINKER:
	case AR_MF_INVALID:
		break;
	}
	ALICE_ERROR("Invalid manifest type");
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

void ar_pack(const char *manifest)
{
	struct ar_manifest *mf = ar_parse_manifest(manifest);
	const char *ext = file_extension(mf->output_path->text);
	if (!ext || strcasecmp(ext, "afa"))
		ALICE_ERROR("Only .afa archives supported");

	// paths are relative to manifest location
	char *old_cwd = xmalloc(2048);
	old_cwd = getcwd(old_cwd, 2048);
	chdir_to_file(manifest);

	size_t nr_files;
	struct ar_file_spec **files = manifest_to_file_list(mf, &nr_files);
	write_afa(mf->output_path, files, nr_files);

	free_manifest(mf);
	for (size_t i = 0; i < nr_files; i++) {
		free_string(files[i]->path);
		free_string(files[i]->name);
		free(files[i]);
	}
	free(files);
	chdir(old_cwd);
	free(old_cwd);
}

int command_ar_pack(int argc, char *argv[])
{
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ar_pack);
		if (c == -1)
			break;
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		USAGE_ERROR(&cmd_ar_extract, "Wrong number of arguments");
	}

	ar_pack(argv[0]);
	return 0;
}

struct command cmd_ar_pack = {
	.name = "pack",
	.usage = "[options...] <manifest-file>",
	.description = "Create an archive file",
	.parent = &cmd_ar,
	.fun = command_ar_pack,
	.options = {
		{ 0 }
	}
};
