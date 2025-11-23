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
#include "system4/string.h"
#include "alice.h"
#include "alice/flat.h"
#include "cli.h"

static struct string *get_output_path(const char *output_file, const char *input_file)
{
	if (output_file)
		return make_string(output_file, strlen(output_file));
	return replace_extension(output_file, "x");
}

enum {
	LOPT_OUTPUT = 256,
	LOPT_PNG,
};

int command_flat_extract(int argc, char *argv[])
{
	char *output_file = NULL;
	bool png = false;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_flat_extract);
		if (c == -1)
			break;
		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case LOPT_PNG:
			png = true;
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
	size_t size;
	uint8_t *data = file_read(argv[0], &size);
	if (!data)
		ALICE_ERROR("file_read(\"%s\") failed: %s", argv[0], strerror(errno));

	int error;
	struct flat *flat = flat_open(data, size, &error);
	if (!flat)
		ALICE_ERROR("Failed to read .flat file \"%s\"", argv[0]);
	flat->needs_free = true;

	// write manifest
	struct string *out_file = get_output_path(output_file, argv[0]);
	flat_extract(flat, out_file->text, png);

	free_string(out_file);
	flat_free(flat);

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
		{ "png",    0,   "Output images as .png format", no_argument, LOPT_PNG },
		{ 0 }
	}
};
