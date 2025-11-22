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
#include "system4.h"
#include "system4/archive.h"
#include "system4/flat.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/flat.h"
#include "cli.h"

enum {
	LOPT_OUTPUT = 256,
};

int command_flat_build(int argc, char *argv[])
{
	struct string *mf_output_file = NULL;
	struct string *output_file = NULL;
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_flat_build);
		if (c == -1)
			break;
		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = make_string(optarg, strlen(optarg));
			break;
		}
	}

	argc -= optind;
	argv += optind;

	// check argument count
	if (argc != 1) {
		USAGE_ERROR(&cmd_flat_build, "Wrong number of arguments");
	}

	// build flat object from manifest
	struct flat *flat = flat_build(argv[0], &mf_output_file);
	if (!output_file) {
		if (mf_output_file) {
			struct string *dir = cstr_to_string(path_dirname(argv[0]));
			output_file = string_path_join(dir, mf_output_file->text);
			free_string(dir);
		} else {
			output_file = replace_extension(argv[0], "flat");
		}
	}

	// write flat file
	FILE *out = checked_fopen(output_file->text, "wb");
	checked_fwrite(flat->data, flat->data_size, out);
	fclose(out);

	flat_free(flat);
	if (mf_output_file)
		free_string(mf_output_file);
	free_string(output_file);
	return 0;
}

struct command cmd_flat_build = {
	.name = "build",
	.usage = "[options] <input-file>",
	.description = "Build a .flat file",
	.parent = &cmd_flat,
	.fun = command_flat_build,
	.options = {
		{ "output", 'o', "Specify the output file", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
