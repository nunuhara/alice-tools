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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "little_endian.h"
#include "system4.h"
#include "alice.h"
#include "alice/acx.h"
#include "cli.h"

enum {
	LOPT_OUTPUT = 256,
};

int command_acx_dump(int argc, char *argv[])
{
	char *output_file = NULL;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_acx_dump);
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
		USAGE_ERROR(&cmd_acx_dump, "Wrong number of arguments");
	}

	int error = ACX_SUCCESS;
	FILE *out = alice_open_output_file(output_file);

	struct acx *acx = acx_load(argv[0], &error);
	if (error == ACX_ERROR_FILE) {
		ALICE_ERROR("Failed to load .acx file \"%s\": %s", argv[0], strerror(errno));
	} else if (error != ACX_SUCCESS) {
		ALICE_ERROR("Invalid .acx file: \"%s\"", argv[0]);
	}
	acx_dump(out, acx);
	fclose(out);
	acx_free(acx);

	return 0;
}

struct command cmd_acx_dump  = {
	.name = "dump",
	.usage = "[options...] <input-file>",
	.description = "Dump the contents of a .acx file to .csv",
	.parent = &cmd_acx,
	.fun = command_acx_dump,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
