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
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <iconv.h>
#include <libgen.h>
#include <unistd.h>
#include "system4.h"
#include "system4/ex.h"
#include "system4/string.h"
#include "alice.h"

void ex_write(FILE *out, struct ex *ex);
extern bool columns_first;
extern unsigned long yex_line;

enum {
	LOPT_OUTPUT = 256,
	LOPT_OLD,
};

int command_ex_build(int argc, char *argv[])
{
	const char *output_file = NULL;
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ex_build);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case LOPT_OLD:
			columns_first = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		USAGE_ERROR(&cmd_ex_build, "Wrong number of arguments.");
	}

	FILE *out = alice_open_output_file(output_file);

	char *input_file = conv_cmdline_utf8(argv[0]);
	struct ex *ex = ex_parse_file(input_file);
	free(input_file);

	if (!ex) {
		ALICE_ERROR("failed to parse .txtex file: '%s'", argv[0]);
	}
	ex_write(out, ex);
	fclose(out);
	ex_free(ex);
	return 0;
}

struct command cmd_ex_build = {
	.name = "build",
	.usage = "[options...] <input-file>",
	.description = "Build a .ex file",
	.parent = &cmd_ex,
	.fun = command_ex_build,
	.options = {
		{ "output", 'o', "Specify the output file path",   required_argument, LOPT_OUTPUT },
		{ "old",    0,   "Use for pre-Evenicle .ex files", no_argument,       LOPT_OLD },
		{ 0 }
	}
};
