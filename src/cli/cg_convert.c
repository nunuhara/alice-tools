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
#include "system4/cg.h"
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"
#include "cli.h"

enum {
	LOPT_TO = 256,
};

static enum cg_type parse_cg_format(const char *fmt)
{
	if (!strcasecmp(fmt, "qnt"))
		return ALCG_QNT;
	if (!strcasecmp(fmt, "ajp"))
		ALICE_ERROR(".ajp output not supported");
	if (!strcasecmp(fmt, "png"))
		return ALCG_PNG;
	if (!strcasecmp(fmt, "pms8"))
		ALICE_ERROR(".pms output not supported");
	if (!strcasecmp(fmt, "pms16"))
		ALICE_ERROR(".pms output not supported");
	if (!strcasecmp(fmt, "webp"))
		return ALCG_WEBP;
	if (!strcasecmp(fmt, "dcf"))
		ALICE_ERROR(".dcf output not supported");
	ALICE_ERROR("Unknown CG format: %s", fmt);
}

int command_cg_convert(int argc, char *argv[])
{
	enum cg_type output_format = ALCG_UNKNOWN;
	struct string *output_file;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_cg_convert);
		if (c == -1)
			break;
		switch (c) {
		case 't':
		case LOPT_TO:
			output_format = parse_cg_format(optarg);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	// check argument count
	if (argc < 1 || argc > 2)
		USAGE_ERROR(&cmd_cg_convert, "Wrong number of arguments");

	if (argc == 1) {
		if (output_format == ALCG_UNKNOWN)
			ALICE_ERROR("No output format specified");
		output_file = replace_extension(argv[0], cg_file_extension(output_format));
	} else if (argc == 2) {
		if (output_format == ALCG_UNKNOWN)
			output_format = parse_cg_format(file_extension(argv[1]));
		output_file = cstr_to_string(argv[1]);
	} else {
		USAGE_ERROR(&cmd_cg_convert, "Wrong number of arguments");
	}

	// open input CG
	struct cg *in = cg_load_file(argv[0]);
	if (!in)
		ALICE_ERROR("Failed to read input CG: %s", argv[0]);

	// encode/write output CG
	FILE *out = checked_fopen(output_file->text, "wb");
	if (!cg_write(in, output_format, out))
		ALICE_ERROR("cg_write failed");

	cg_free(in);
	fclose(out);
	free_string(output_file);
	return 0;
}

struct command cmd_cg_convert = {
	.name = "convert",
	.usage = "[options...] <input-file> <output-file>",
	.description = "Convert a CG file to another format",
	.parent = &cmd_cg,
	.fun = command_cg_convert,
	.options = {
		{ "to",   't', "Specify output format", required_argument, LOPT_TO },
		{ 0 }
	}
};


