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
#include "alice.h"
#include "alice/ar.h"
#include "cli.h"

enum {
	LOPT_AFA_VERSION = 256,
	LOPT_BACKSLASH
};

int command_ar_pack(int argc, char *argv[])
{
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	int afa_version = 2;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ar_pack);
		if (c == -1)
			break;

		switch (c) {
		case LOPT_AFA_VERSION:
			afa_version = atoi(optarg);
			if (afa_version < 1 || afa_version > 2)
				ALICE_ERROR("Unsupported .afa version: %d", afa_version);
			break;
		case LOPT_BACKSLASH:
			ar_set_path_separator('\\');
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		USAGE_ERROR(&cmd_ar_extract, "Wrong number of arguments");
	}

	ar_pack(argv[0], afa_version);
	return 0;
}

struct command cmd_ar_pack = {
	.name = "pack",
	.usage = "[options...] <manifest-file>",
	.description = "Create an archive file",
	.parent = &cmd_ar,
	.fun = command_ar_pack,
	.options = {
		{ "afa-version", 0, "Specify the .afa version (1 or 2)", required_argument, LOPT_AFA_VERSION },
		{ "backslash", 0, "Use backslash as the path separator", no_argument, LOPT_BACKSLASH },
		{ 0 }
	}
};
