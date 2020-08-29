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
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <iconv.h>
#include <libgen.h>
#include <unistd.h>
#include "system4.h"
#include "system4/ex.h"
#include "system4/string.h"
#include "ast.h"

void ex_write(FILE *out, struct ex *ex);
extern bool columns_first;

iconv_t output_conv;

char *convert_text(iconv_t cd, const char *str);

char *encode_text(const char *str)
{
	return convert_text(output_conv, str);
}

static void usage(void)
{
	puts("Usage: exbuild [options...] input-file");
	puts("    Build EX files.");
	puts("");
	puts("    -h, --help                     Display this message and exit");
	puts("    -o, --output                   Set the output file path");
	puts("        --old                      Use this for pre-Evenicle .ex files");
	puts("        --input-encoding <enc>     Specify the encoding of the input file (default: UTF-8)");
	puts("        --output-encoding <enc>    Specify the encoding of the output file (default: CP932)");
}

enum {
	LOPT_HELP = 256,
	LOPT_OUTPUT,
	LOPT_OLD,
	LOPT_INPUT_ENCODING,
	LOPT_OUTPUT_ENCODING,
};

int main(int argc, char *argv[])
{
	const char *output_file = NULL;
	const char *input_encoding = "UTF-8";
	const char *output_encoding = "CP932";

	while (1) {
		static struct option long_options[] = {
			{ "help",            no_argument,       0, LOPT_HELP },
			{ "output",          required_argument, 0, LOPT_OUTPUT },
			{ "old",             no_argument,       0, LOPT_OLD },
			{ "input-encoding",  required_argument, 0, LOPT_INPUT_ENCODING },
			{ "output-encoding", required_argument, 0, LOPT_OUTPUT_ENCODING },
		};
		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "ho:", long_options, &option_index);
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
		case LOPT_OLD:
			columns_first = true;
			break;
		case LOPT_INPUT_ENCODING:
			input_encoding = optarg;
			break;
		case LOPT_OUTPUT_ENCODING:
			output_encoding = optarg;
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

	FILE *out = stdout;
	if (output_file)
		out = fopen(output_file, "wb");
	if (!out)
		ERROR("fopen failed: %s", strerror(errno));

	FILE *in;
	if (!strcmp(argv[0], "-")) {
		in = stdin;
	} else {
		in = fopen(argv[0], "rb");
		chdir(dirname(argv[0]));
	}
	if (!in)
		ERROR("fopen failed: %s", strerror(errno));

	if ((output_conv = iconv_open(output_encoding, input_encoding)) == (iconv_t)-1) {
		ERROR("iconv_open: %s", strerror(errno));
	}

	struct ex *ex = ex_parse(in);
	ex_write(out, ex);
	ex_free(ex);
	return 0;
}
