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

enum ar_filetype ar_parse_filetype(struct string *str)
{
	if (!strcasecmp(str->text, "PNG"))
		return AR_FT_PNG;
	if (!strcasecmp(str->text, "PMS"))
		return AR_FT_PMS;
	if (!strcasecmp(str->text, "QNT"))
		return AR_FT_QNT;
	if (!strcasecmp(str->text, "WEBP"))
		return AR_FT_WEBP;
	if (!strcasecmp(str->text, "X"))
		return AR_FT_X;
	if (!strcasecmp(str->text, "TXTEX"))
		return AR_FT_TXTEX;
	if (!strcasecmp(str->text, "EX"))
		return AR_FT_EX;
	if (!strcasecmp(str->text, "PACTEX"))
		return AR_FT_PACTEX;
	if (!strcasecmp(str->text, "FLAT"))
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

static void make_alicepack_manifest(struct ar_manifest *dst, ar_row_list *rows)
{
	dst->type = AR_MF_ALICEPACK;
	dst->alicepack = xcalloc(dst->nr_rows, sizeof(struct alicepack_line));
	for (size_t i = 0; i < dst->nr_rows; i++) {
		ar_string_list *row = kv_A(*rows, i);
		if (kv_size(*row) != 1)
			ALICE_ERROR("line %d: Too many columns", (int)i+2);
		dst->alicepack[i].filename = kv_A(*row, 0);
		kv_destroy(*row);
		free(row);
	}
}

static void make_batchpack_manifest(struct ar_manifest *dst, ar_row_list *rows)
{
	dst->type = AR_MF_BATCHPACK;
	dst->batchpack = xcalloc(dst->nr_rows, sizeof(struct batchpack_line));
	for (size_t i = 0; i < dst->nr_rows; i++) {
		ar_string_list *row = kv_A(*rows, i);
		if (kv_size(*row) != 4) {
			ALICE_ERROR("line %d: wrong number of columns", (int)i+2);
		}
		dst->batchpack[i].src = kv_A(*row, 0);
		dst->batchpack[i].src_fmt = ar_parse_filetype(kv_A(*row, 1));
		dst->batchpack[i].dst = kv_A(*row, 2);
		dst->batchpack[i].dst_fmt = ar_parse_filetype(kv_A(*row, 3));

		free_string(kv_A(*row, 1));
		free_string(kv_A(*row, 3));
		kv_destroy(*row);
		free(row);
	}
}

static void make_alicecg2_manifest(struct ar_manifest *dst, ar_row_list *rows)
{
	dst->type = AR_MF_ALICECG2;
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
	parse_manifest_options(mf, options);
	mf->output_path = output_path;
	mf->nr_rows = kv_size(*rows);

	switch (ar_parse_manifest_type(magic)) {
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
