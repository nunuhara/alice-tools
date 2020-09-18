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
#include "little_endian.h"
#include "system4.h"
#include "system4/acx.h"
#include "system4/string.h"
#include "alice.h"

enum {
	LOPT_OUTPUT = 256,
};

static void write_string(FILE *out, const char *str)
{
	char *u = conv_output(str);

	fputc('"', out);
	for (int i = 0; u[i]; i++) {
		switch (u[i]) {
		case '"':  fprintf(out, "\\\""); break;
		case '\n': fprintf(out, "\\n");  break;
		default:   fputc(u[i], out);     break;
		}
	}
	fputc('"', out);

	free(u);
}

static void acx_dump(FILE *out, struct acx *acx)
{
	for (int col = 0; col < acx->nr_columns; col++) {
		if (col > 0)
			fputc(',', out);
		switch (acx->column_types[col]) {
		case ACX_INT:    fprintf(out, "int"); break;
		case ACX_STRING: fprintf(out, "string"); break;
		default:
			WARNING("Unknown column type: %d", acx->column_types[col]);
			fprintf(out, "%d", acx->column_types[col]);
			break;
		}
	}
	fputc('\n', out);

	for (int line = 0; line < acx->nr_lines; line++) {
		for (int col = 0; col < acx->nr_columns; col++) {
			if (col > 0)
				fputc(',', out);
			if (acx->column_types[col] == ACX_STRING) {
				write_string(out, acx_get_string(acx, line, col)->text);
			} else {
				fprintf(out, "%d", acx_get_int(acx, line, col));
			}
		}
		fputc('\n', out);
	}
}

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

	FILE *out = alice_open_output_file(output_file);
	struct acx *acx = acx_load(argv[0]);
	if (!acx)
		ALICE_ERROR("Failed to load ACX data from %s", argv[0]);
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
