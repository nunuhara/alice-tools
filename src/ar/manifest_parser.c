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
#include "system4/string.h"
#include "alice.h"
#include "alice-ar.h"

static enum ar_manifest_type ar_parse_manifest_type(struct string *str)
{
	if (!strcasecmp(str->text, "#ALICEPACK"))
		return AR_MF_ALICEPACK;
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

struct ar_manifest *ar_make_manifest(struct string *magic, struct string *output_path, ar_row_list *rows)
{
	struct ar_manifest *mf = xcalloc(1, sizeof(struct ar_manifest));
	mf->output_path = output_path;
	mf->nr_rows = kv_size(*rows);

	switch (ar_parse_manifest_type(magic)) {
	case AR_MF_ALICEPACK:
		make_alicepack_manifest(mf, rows);
		break;
	case AR_MF_ALICECG2:
		ALICE_ERROR("ALICECG2 manifest not supported");
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
