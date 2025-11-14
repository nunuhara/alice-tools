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
#include "system4.h"
#include "system4/cg.h"
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ar.h"

static enum cg_type ar_parse_image_format(struct string *str)
{
	if (!strcasecmp(str->text, "PNG"))
		return ALCG_PNG;
	if (!strcasecmp(str->text, "PMS"))
		return ALCG_PMS16;
	if (!strcasecmp(str->text, "QNT"))
		return ALCG_QNT;
	if (!strcasecmp(str->text, "WEBP"))
		return ALCG_WEBP;
	return ALCG_UNKNOWN;
}

enum ar_filetype ar_parse_filetype(const char *str)
{
	if (!strcasecmp(str, "PNG"))
		return AR_FT_PNG;
	if (!strcasecmp(str, "PMS"))
		return AR_FT_PMS;
	if (!strcasecmp(str, "QNT"))
		return AR_FT_QNT;
	if (!strcasecmp(str, "WEBP"))
		return AR_FT_WEBP;
	if (!strcasecmp(str, "X"))
		return AR_FT_X;
	if (!strcasecmp(str, "TXTEX"))
		return AR_FT_TXTEX;
	if (!strcasecmp(str, "EX"))
		return AR_FT_EX;
	if (!strcasecmp(str, "PACTEX"))
		return AR_FT_PACTEX;
	if (!strcasecmp(str, "FLAT"))
		return AR_FT_FLAT;
	return AR_FT_UNKNOWN;
}

static enum ar_manifest_type ar_parse_manifest_type(struct string *str)
{
	if (!strcasecmp(str->text, "#ALICEPACK"))
		return AR_MF_ALICEPACK;
	if (!strcasecmp(str->text, "#BATCHPACK"))
		return AR_MF_BATCHPACK;
	if (!strcasecmp(str->text, "#ALICECG2"))
		return AR_MF_ALICECG2;
	if (!strcasecmp(str->text, "#NL5"))
		return AR_MF_NL5;
	if (!strcasecmp(str->text, "#WavLinker"))
		return AR_MF_WAVLINKER;
	return AR_MF_INVALID;
}

static enum ar_filetype get_filetype_from_name(const char *name)
{
	return ar_parse_filetype(file_extension(name));
}

static void make_alicepack_manifest(struct ar_manifest *dst, ar_row_list *rows)
{
	dst->alicepack = xcalloc(dst->nr_rows, sizeof(struct alicepack_line));
	for (size_t i = 0; i < dst->nr_rows; i++) {
		ar_string_list *row = kv_A(*rows, i);
		struct alicepack_line *line = &dst->alicepack[i];
		if (kv_size(*row) == 2) {
			line->src_fmt = get_filetype_from_name(kv_A(*row, 0)->text);
			if (line->src_fmt == AR_FT_UNKNOWN)
				ALICE_ERROR("Unrecognized src file format for conversion: %s",
						kv_A(*row, 0)->text);
			line->dst_fmt = ar_parse_filetype(kv_A(*row, 1)->text);
			if (line->dst_fmt == AR_FT_UNKNOWN)
				ALICE_ERROR("Unrecognized dst file format for conversion: %s",
						kv_A(*row, 1)->text);
			free_string(kv_A(*row, 1));
		} else if (kv_size(*row) != 1) {
			ALICE_ERROR("line %d: Too many columns", (int)i+2);
		}
		if (dst->src_dir) {
			line->src = path_join_string(dst->src_dir, kv_A(*row, 0));
			line->dst = kv_A(*row, 0);
		} else {
			line->src = kv_A(*row, 0);
			line->dst = string_ref(dst->alicepack[i].src);
		}
		if (line->dst_fmt != AR_FT_UNKNOWN) {
			struct string *tmp = line->dst;
			line->dst = replace_extension(tmp->text, ar_ft_extension(line->dst_fmt));
			free_string(tmp);
		}
		kv_destroy(*row);
		free(row);
	}
}

static void make_batchpack_manifest(struct ar_manifest *dst, ar_row_list *rows)
{
	dst->batchpack = xcalloc(dst->nr_rows, sizeof(struct batchpack_line));
	for (size_t i = 0; i < dst->nr_rows; i++) {
		ar_string_list *row = kv_A(*rows, i);
		if (kv_size(*row) != 4) {
			ALICE_ERROR("line %d: wrong number of columns", (int)i+2);
		}
		dst->batchpack[i].src = kv_A(*row, 0);
		dst->batchpack[i].src_fmt = ar_parse_filetype(kv_A(*row, 1)->text);
		dst->batchpack[i].dst = kv_A(*row, 2);
		dst->batchpack[i].dst_fmt = ar_parse_filetype(kv_A(*row, 3)->text);

		free_string(kv_A(*row, 1));
		free_string(kv_A(*row, 3));
		kv_destroy(*row);
		free(row);
	}
}

static void make_alicecg2_manifest(struct ar_manifest *dst, ar_row_list *rows)
{
	dst->alicecg2 = xcalloc(dst->nr_rows, sizeof(struct alicecg2_line));
	for (size_t i = 0; i < dst->nr_rows; i++) {
		ar_string_list *row = kv_A(*rows, i);
		if (kv_size(*row) != 5)
			ALICE_ERROR("line %d: wrong number of columns", (int)i+2);
		// TODO: parse file number (for ALD)
		dst->alicecg2[i].file_no = 0;
		dst->alicecg2[i].src = kv_A(*row, 1);
		dst->alicecg2[i].src_fmt = ar_parse_image_format(kv_A(*row, 2));
		dst->alicecg2[i].dst = kv_A(*row, 3);
		dst->alicecg2[i].dst_fmt = ar_parse_image_format(kv_A(*row, 4));

		free_string(kv_A(*row, 0));
		free_string(kv_A(*row, 2));
		free_string(kv_A(*row, 4));
		kv_destroy(*row);
		free(row);
	}
}

static void parse_manifest_options(struct ar_manifest *mf, ar_string_list *options)
{
	mf->afa_version = -1;
	mf->backslash = false;
	for (size_t i = 0; i < kv_size(*options); i++) {
		struct string *opt = kv_A(*options, i);
		if (!strcmp(opt->text, "--backslash")) {
			mf->backslash = true;
		} else if (!strncmp(opt->text, "--afa-version=", 14)) {
			int version = atoi(opt->text + 14);
			if (version < 1 || version > 2)
				WARNING("Ignoring invalid afa version: '%s'", opt->text + 14);
			else
				mf->afa_version = version;
		} else if (!strncmp(opt->text, "--src-dir=", 10)) {
			if (mf->type != AR_MF_ALICEPACK) {
				WARNING("Ignoring --src-dir option (only valid for ALICEPACK archives)");
			} else {
				mf->src_dir = make_string(opt->text+10, strlen(opt->text)-10);
			}
		} else {
			WARNING("Unrecognized manifest option: '%s'", opt->text);
		}

		free_string(opt);
	}

	kv_destroy(*options);
	free(options);
}

struct ar_manifest *ar_make_manifest(struct string *magic, ar_string_list *options, struct string *output_path, ar_row_list *rows)
{
	struct ar_manifest *mf = xcalloc(1, sizeof(struct ar_manifest));
	mf->type = ar_parse_manifest_type(magic);
	mf->output_path = output_path;
	mf->nr_rows = kv_size(*rows);
	parse_manifest_options(mf, options);

	switch (mf->type) {
	case AR_MF_ALICEPACK:
		make_alicepack_manifest(mf, rows);
		break;
	case AR_MF_BATCHPACK:
		make_batchpack_manifest(mf, rows);
		break;
	case AR_MF_ALICECG2:
		make_alicecg2_manifest(mf, rows);
		break;
	case AR_MF_NL5:
		ALICE_ERROR("NL5 manifest not supported");
	case AR_MF_WAVLINKER:
		ALICE_ERROR("WavLinker manifest not supported");
	case AR_MF_INVALID:
	default:
		ALICE_ERROR("Invalid manifest type");
	}

	kv_destroy(*rows);
	free(rows);
	free_string(magic);
	return mf;
}
