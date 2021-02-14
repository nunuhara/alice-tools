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

#ifndef ALICEAR_H_
#define ALICEAR_H_

#include <stddef.h>
#include "kvec.h"
#include "system4/cg.h"

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
	AR_FT_EX,
	AR_FT_PACTEX,
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
	int class;
	struct string *src;
	int compression_rate;
	int link_no_offset;
};

struct ar_manifest {
	enum ar_manifest_type type;
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

struct ar_file_spec {
	struct string *path;
	struct string *name;
};

kv_decl(ar_string_list, struct string*);
kv_decl(ar_row_list, ar_string_list*);

struct ar_manifest *ar_make_manifest(struct string *magic, struct string *output_path, ar_row_list *rows);
struct ar_manifest *ar_parse_manifest(const char *path);

#endif /* ALICEAR_H_ */
