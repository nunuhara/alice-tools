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

#include <stdlib.h>
#include "system4.h"
#include "system4/ex.h"
#include "alice.h"
#include "alice/ex.h"
#include "cli.h"

extern bool columns_first;

enum {
	LOPT_OUTPUT = 256,
	LOPT_OLD,
	LOPT_EXTRACT,
	LOPT_REPLACE,
};

static int command_ex_edit(int argc, char *argv[]) {
	const char *output_file = NULL;
	bool extract = false;
	bool replace = false;
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ex_edit);
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
		case 'e':
		case LOPT_EXTRACT:
			extract = true;
			break;
		case 'r':
		case LOPT_REPLACE:
			replace = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 2) {
		USAGE_ERROR(&cmd_ex_edit, "Wrong number of arguments");
	}
	if (extract && replace) {
		USAGE_ERROR(&cmd_ex_edit, "Cannot extract and replace in the same operation");
	}

	FILE *out = alice_open_output_file(output_file);

	struct ex *base = ex_read_file(argv[0]);
	if (!base) {
		ALICE_ERROR("failed to read .ex file: %s", argv[0]);
	}

	struct ex *edit = ex_parse_file(argv[1]);
	if (!edit) {
		ALICE_ERROR("failed to parse .txtex file: %s", argv[1]);
	}

	if (extract) {
		struct ex *extract = ex_extract_append(base, edit);
		ex_write(out, extract);
		ex_free(extract);
	} else if (replace) {
		ex_replace(base, edit);
		ex_write(out, base);
	} else {
		ex_append(base, edit);
		ex_write(out, base);
	}
	ex_free(base);
	ex_free(edit);
	fclose(out);
	return 0;
}

struct command cmd_ex_edit = {
	.name = "edit",
	.usage = "[options...] <ex-file> <txtex-file>",
	.description = "Edit a .ex file",
	.parent = &cmd_ex,
	.fun = command_ex_edit,
	.options = {
		{ "output",  'o', "Specify the output file path",      required_argument, LOPT_OUTPUT },
		{ "old",       0, "Use for pre-Evenicle .ex files",    no_argument,       LOPT_OLD },
		{ "extract", 'e', "Only write modified objects",       no_argument,       LOPT_EXTRACT },
		{ "replace", 'r', "Replace data instead of appending", no_argument,       LOPT_REPLACE },
		{ 0 }
	}
};
