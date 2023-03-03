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
#include <stdlib.h>
#include <stdio.h>
#include "alice.h"
#include "alice/acx.h"
#include "cli.h"

enum {
	LOPT_OUTPUT = 256,
};

// csv_parser.y
extern struct acx *acx_parse(const char *path);
extern unsigned long csv_line;

int command_acx_build(int argc, char *argv[])
{
	const char *output_file = NULL;
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_acx_build);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		USAGE_ERROR(&cmd_acx_build, "Wrong number of arguments.");
	}

	current_line_nr = &csv_line;
	current_file_name = (const char**)&argv[0];

	FILE *out = alice_open_output_file(output_file);
	struct acx *acx = acx_parse(argv[0]);
	acx_write(out, acx);
	acx_free(acx);
	fclose(out);
	return 0;
}

struct command cmd_acx_build = {
	.name = "build",
	.usage = "[options...] <input-file>",
	.description = "Build a .acx file from a .csv",
	.parent = &cmd_acx,
	.fun = command_acx_build,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
