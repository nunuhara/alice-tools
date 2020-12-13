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
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"
#include "alice-ar.h"

void write_afa(struct string *filename, struct ar_file_spec **files, size_t nr_files);

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

static struct string *mkpath(const struct string *dir, const char *file)
{
	struct string *path = string_dup(dir);
	if (dir->text[dir->size-1] != '/')
		string_push_back(&path, '/');
	string_append_cstr(&path, file, strlen(file));
	return path;
}

static struct string *replace_extension(const struct string *file, const char *ext)
{
	const char *src_ext = strrchr(file->text, '.');
	if (!src_ext) {
		struct string *dst = string_dup(file);
		string_push_back(&dst, '.');
		string_append_cstr(&dst, ext, strlen(ext));
		return dst;
	}
	size_t base_len = (src_ext+1) - file->text;
	struct string *dst = make_string(file->text, base_len);
	string_append_cstr(&dst, ext, strlen(ext));
	return dst;
}

static void convert_images(struct alicecg2_line *line)
{
	struct dirent *dir;
	DIR *d = checked_opendir(line->src->text);

	while ((dir = readdir(d)) != NULL) {
		if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
			continue;

		struct stat src_s;
		struct string *src_path = mkpath(line->src, dir->d_name);
		struct string *dst_tmp = mkpath(line->dst, dir->d_name);
		struct string *dst_path = replace_extension(dst_tmp, cg_file_extensions[line->dst_fmt]);
		free_string(dst_tmp);
		checked_stat(src_path->text, &src_s);

		if (!S_ISREG(src_s.st_mode)) {
			WARNING("Skipping \"%s\": not a regular file", src_path->text);
			goto loop_next;
		}

		// skip if dst exists and is newer than src
		if (file_exists(dst_path->text)) {
			struct stat dst_s;
			checked_stat(dst_path->text, &dst_s);
			if (src_s.st_mtime < dst_s.st_mtime) {
				goto loop_next;
			}
		}

		NOTICE("%s -> %s", src_path->text, dst_path->text);

		// skip transcode if src/dst formats match
		if (line->src_fmt == line->dst_fmt) {
			if (!file_copy(src_path->text, dst_path->text))
				ALICE_ERROR("failed to copy file \"%s\": %s", dst_path->text, strerror(errno));
			goto loop_next;
		}

		// transcode image
		struct cg *cg = cg_load_file(src_path->text);
		if (!cg) {
			goto loop_next;
		}
		FILE *f = checked_fopen(dst_path->text, "wb");
		if (!cg_write(cg, line->dst_fmt, f))
			ALICE_ERROR("failed to encode file \"%s\"", dst_path->text);
		fclose(f);
		cg_free(cg);
	loop_next:
		free_string(src_path);
		free_string(dst_path);
	}
	closedir(d);
}

static struct ar_file_spec **alicecg2_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	kvec_t(struct ar_file_spec*) files;
	kv_init(files);
	for (size_t i = 0; i < mf->nr_rows; i++) {
		struct string *src = mf->alicecg2[i].src;
		struct string *dst = mf->alicecg2[i].dst;
		if (!is_directory(src->text))
			ALICE_ERROR("line %d: \"%s\" is not a directory", (int)i+2, src->text);
		if (!is_directory(dst->text))
			ALICE_ERROR("line %d: \"%s\" is not a directory", (int)i+2, dst->text);
		convert_images(mf->alicecg2+i);

		// add all files in dst to file list
		// TODO: filter by file extension?
		struct dirent *dir;
		DIR *d = checked_opendir(dst->text);
		while ((dir = readdir(d)) != NULL) {
			if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
				continue;
			struct ar_file_spec *spec = xmalloc(sizeof(struct ar_file_spec));
			spec->path = mkpath(dst, dir->d_name);
			spec->name = make_string(dir->d_name, strlen(dir->d_name));
			kv_push(struct ar_file_spec*, files, spec);
		}
		closedir(d);
	}

	*size_out = files.n;
	return files.a;
}

static struct ar_file_spec **manifest_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	switch (mf->type) {
	case AR_MF_ALICEPACK:
		return alicepack_to_file_list(mf, size_out);
	case AR_MF_ALICECG2:
		return alicecg2_to_file_list(mf, size_out);
	case AR_MF_NL5:
	case AR_MF_WAVLINKER:
	case AR_MF_INVALID:
	default:
		ALICE_ERROR("Invalid manifest type");
	}
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
	case AR_MF_NL5:
	case AR_MF_WAVLINKER:
	case AR_MF_INVALID:
	default:
		ALICE_ERROR("Invalid manifest type");
	}
	free_string(mf->output_path);
	free(mf);
}

int command_ar_pack(int argc, char *argv[])
{
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

	struct ar_manifest *mf = ar_parse_manifest(argv[0]);
	const char *ext = file_extension(mf->output_path->text);
	if (!ext || strcasecmp(ext, "afa"))
		ALICE_ERROR("Only .afa archives supported");

	// paths are relative to manifest location
	char *tmp = strdup(argv[0]);
	chdir(dirname(tmp));
	free(tmp);

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
