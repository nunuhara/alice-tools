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

#ifndef ALICE_AR_H_
#define ALICE_AR_H_

#include <stddef.h>
#include "kvec.h"
#include "system4/cg.h"

enum {
	AR_RAW = 1,
	AR_FORCE = 2,
	AR_IMAGES_ONLY = 4,
};

#define AR_IMGENC(flags) ((flags & 0xFF000000UL) >> 24)
#define AR_IMGENC_BITS(enc) (enc << 24)

enum archive_type {
	AR_AAR,
	AR_ALD,
	AR_AFA,
	AR_AFA3,
	AR_FLAT,
	AR_DLF,
	AR_ALK,
};

enum ar_manifest_type {
	AR_MF_INVALID,
	AR_MF_ALICEPACK,
	AR_MF_BATCHPACK,
	AR_MF_ALICECG2,
	AR_MF_NL5,
	AR_MF_WAVLINKER,
};

enum ar_filetype {
	AR_FT_UNKNOWN,
	AR_FT_PNG,
	AR_FT_PMS,
	AR_FT_QNT,
	AR_FT_WEBP,
	AR_FT_X,
	AR_FT_TXTEX,
	AR_FT_EX,
	AR_FT_PACTEX,
	AR_FT_FLAT,
};

// Custom manifest format (simple)
// #ALICEPACK
// dst
// filename[, dst_fmt[, fmt_opts...]] ; allows optional transcode
// ...
struct alicepack_line {
	struct string *filename;
};

// Custom manifest format (batch conversion)
// #BATCHPACK
// dst
// src,src_fmt,dst,dst_fmt
struct batchpack_line {
	struct string *src;
	enum ar_filetype src_fmt;
	struct string *dst;
	enum ar_filetype dst_fmt;
};

// ALICECG2: file, src, src_fmt, dst, dst_fmt
struct alicecg2_line {
	int file_no;
	struct string *src;
	enum cg_type src_fmt;
	struct string *dst;
	enum cg_type dst_fmt;
};

// NL5: file, link_no, filename
struct nl5_line {
	int file_no;
	int link_no;
	struct string *filename;
};

// WavLinker:  file (char), classification (0 only), wav folder, compression rate, linkno offset
struct wavlinker_line {
	int file_no;
	int type;
	struct string *src;
	int compression_rate;
	int link_no_offset;
};

struct ar_manifest {
	enum ar_manifest_type type;
	int afa_version;
	bool backslash;
	struct string *output_path;
	size_t nr_rows;
	union {
		struct alicepack_line *alicepack;
		struct batchpack_line *batchpack;
		struct alicecg2_line *alicecg2;
		struct nl5_line *nl5;
		struct wavlinker_line *wavlinker;
	};
};

enum ar_file_spec_type {
	AR_FILE_SPEC_DISK,
	AR_FILE_SPEC_MEM,
};

struct ar_file_spec {
	enum ar_file_spec_type type;
	union {
		// AR_FILE_SPEC_DISK
		struct {
			struct string *path;
		} disk;
		// AR_FILE_SPEC_MEM
		struct {
			void *data;
			size_t size;
		} mem;
	};
	struct string *name;
};

enum ar_filetype ar_parse_filetype(struct string *str);
void write_afa(struct string *filename, struct ar_file_spec **files, size_t nr_files, int version);

// extract.c
void ar_extract_all(struct archive *ar, const char *output_file, uint32_t flags);
void ar_extract_file(struct archive *ar, char *file_name, char *output_file, uint32_t flags);
void ar_extract_index(struct archive *ar, int file_index, char *output_file, uint32_t flags);

// open.c
struct archive *open_archive(const char *path, enum archive_type *type, int *error);
struct archive *open_ald_archive(const char *path, int *error, char *(*conv)(const char*));

// pack.c
kv_decl(ar_file_list, struct ar_file_spec*);
kv_decl(ar_string_list, struct string*);
kv_decl(ar_row_list, ar_string_list*);
void ar_set_path_separator(char c);
void ar_pack_manifest(struct ar_manifest *ar, int afa_version);
void ar_to_file_list(struct archive *ar, ar_file_list *files);
void ar_dir_to_file_list(struct string *dir, ar_file_list *files, enum ar_filetype fmt);
void ar_file_spec_free(struct ar_file_spec *spec);
void ar_file_list_free(ar_file_list *list);
void ar_file_list_sort(ar_file_list *list);

void ar_pack(const char *manifest, int afa_version);

struct ar_manifest *ar_make_manifest(struct string *magic, ar_string_list *options,
		struct string *output_path, ar_row_list *rows);
struct ar_manifest *ar_parse_manifest(const char *path);

#endif /* ALICE_AR_H_ */
