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
#include <limits.h>
#include "system4.h"
#include "system4/cg.h"
#include "system4/flat.h"
#include "alice.h"
#include "alice/flat.h"
#include "cli.h"

static char *get_output_path(const char *output_file, const char *input_file)
{
	if (output_file)
		return xstrdup(output_file);

	size_t len = strlen(input_file);
	char *name = xmalloc(len + 3);
	memcpy(name, input_file, len);
	memcpy(name+len, ".x", 3);
	return name;
}

enum {
	LOPT_OUTPUT = 256,
};

int command_flat_extract(int argc, char *argv[])
{
	char *output_file = NULL;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_flat_extract);
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

	// check argument count
	if (argc != 1) {
		USAGE_ERROR(&cmd_flat_extract, "Wrong number of arguments");
	}

	// open .flat file
	struct flat_archive *flat;
	int error;
	flat = flat_open_file(argv[0], 0, &error);
	if (!flat) {
		ALICE_ERROR("Opening archive: %s", archive_strerror(error));
	}

	// write manifest
	output_file = get_output_path(output_file, argv[0]);
	flat_extract(flat, output_file);

	free(output_file);
	archive_free(&flat->ar);

	return 0;
}

struct command cmd_flat_extract = {
	.name = "extract",
	.usage = "[options...] <input-file>",
	.description = "Extract the contents of a .flat file",
	.parent = &cmd_flat,
	.fun = command_flat_extract,
	.options = {
		{ "output", 'o', "Specify output file", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
