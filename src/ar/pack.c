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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "system4.h"
#include "system4/archive.h"
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"
#include "alice-ar.h"

void write_afa(struct string *filename, struct string **files, size_t nr_files);

static struct string **alicepack_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	struct string **files = xcalloc(mf->nr_rows, sizeof(struct string*));
	*size_out = mf->nr_rows;

	for (size_t i = 0; i < mf->nr_rows; i++) {
		files[i] = string_ref(mf->alicepack[i].filename);
	}

	return files;
}

static struct string **manifest_to_file_list(struct ar_manifest *mf, size_t *size_out)
{
	switch (mf->type) {
	case AR_MF_ALICEPACK:
		return alicepack_to_file_list(mf, size_out);
	case AR_MF_ALICECG2:
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

	size_t nr_files;
	struct string **files = manifest_to_file_list(mf, &nr_files);
	write_afa(mf->output_path, files, nr_files);

	free_manifest(mf);
	for (size_t i = 0; i < nr_files; i++) {
		free_string(files[i]);
	}
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
