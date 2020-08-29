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
#include "system4/utfsjis.h"

static void usage(void)
{
	puts("Usage: acxdump [options...] input-file");
	puts("    Dump ACX files.");
	puts("");
	puts("    -h, --help                     Display this message and exit");
	puts("    -o, --output                   Set the output file path");
}

enum {
	LOPT_HELP = 256,
	LOPT_OUTPUT,
};

static void write_string(FILE *out, const char *str)
{
	char *u = sjis2utf(str, 0);

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

void acx_dump(FILE *out, struct acx *acx)
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

int main(int argc, char *argv[])
{
	char *output_file = NULL;

	while (1) {
		static struct option long_options[] = {
			{ "help",    no_argument,       0, LOPT_HELP },
			{ "output",  required_argument, 0, LOPT_OUTPUT },
		};
		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "hdo:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case LOPT_HELP:
			usage();
			return 0;
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case '?':
			ERROR("Unknown command line argument");
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		ERROR("Wrong number of arguments.");
	}

	FILE *out = stdin;
	if (output_file)
		out = fopen(output_file, "wb");
	if (!out)
		ERROR("fopen failed: %s", strerror(errno));

	struct acx *acx = acx_load(argv[0]);
	if (!acx)
		ERROR("Failed to load ACX data from %s", argv[0]);
	acx_dump(out, acx);
	fclose(out);
	acx_free(acx);

	return 0;
}
