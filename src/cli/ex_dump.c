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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <iconv.h>
#include "little_endian.h"
#include "system4.h"
#include "system4/ex.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ex.h"
#include "alice/port.h"
#include "cli.h"

enum {
	LOPT_DECRYPT = 256,
	LOPT_OUTPUT,
	LOPT_SPLIT,
};

int command_ex_dump(int argc, char *argv[])
{
	bool decrypt = false;
	bool split = false;
	char *output_file = NULL;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ex_dump);
		if (c == -1)
			break;

		switch (c) {
		case 'd':
		case LOPT_DECRYPT:
			decrypt = true;
			break;
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case 's':
		case LOPT_SPLIT:
			split = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		USAGE_ERROR(&cmd_ex_dump, "Wrong number of arguments.");
	}

	FILE *out = alice_open_output_file(output_file);

	if (decrypt) {
		size_t size;
		uint8_t *buf = ex_decrypt(argv[0], &size, NULL);
		if (fwrite(buf, size, 1, out) != 1)
			ERROR("fwrite failed: %s", strerror(errno));

		free(buf);
		return 0;
	}

	struct ex *ex = ex_read_file(argv[0]);
	if (!ex)
		ALICE_ERROR("ex_read_file(\"%s\") failed", argv[0]);

	if (split) {
		const char *dir;
		if (output_file)
			dir = dirname(output_file);
		else
			dir = ".";
		ex_dump_split(out, ex, dir);
	} else {
		struct port port;
		port_file_init(&port, out);
		ex_dump(&port, ex);
		port_close(&port);
		fclose(out);
	}
	ex_free(ex);

	return 0;
}

struct command cmd_ex_dump = {
	.name = "dump",
	.usage = "[options...] <input-file>",
	.description = "Dump the contents of a .ex file",
	.parent = &cmd_ex,
	.fun = command_ex_dump,
	.options = {
		{ "decrypt", 'd', "Decrypt the .ex file only",            no_argument, LOPT_DECRYPT },
		{ "output",  'o', "Specify the output file path",         required_argument, LOPT_OUTPUT },
		{ "split",   's', "Split the output into multiple files", no_argument, LOPT_SPLIT },
		{ 0 }
	}
};
