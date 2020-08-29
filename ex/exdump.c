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

int ex_dump(FILE *out, struct ex *ex);
int ex_dump_split(FILE *out, struct ex *ex, const char *dir);

iconv_t output_conv;
iconv_t utf8_conv;

char *convert_text(iconv_t cd, const char *str);

char *encode_text(const char *str)
{
	return convert_text(output_conv, str);
}

char *encode_text_utf8(const char *str)
{
	return convert_text(utf8_conv, str);
}

static void usage(void)
{
	puts("Usage: exdump [options...] input-file");
	puts("    Dump EX files.");
	puts("");
	puts("    -h, --help                     Display this message and exit");
	puts("    -d, --decrypt                  Decrypt the EX file only");
	puts("    -o, --output                   Set the output file path");
	puts("    -s, --split                    Split output into multiple files");
	puts("        --input-encoding <enc>     Specify the encoding of the EX file (default: CP932)");
	puts("        --output-encoding <enc>    Specify the encoding of the output file (default UTF-8)");
}

enum {
	LOPT_HELP = 256,
	LOPT_DECRYPT,
	LOPT_OUTPUT,
	LOPT_SPLIT,
	LOPT_INPUT_ENCODING,
	LOPT_OUTPUT_ENCODING,
};

int main(int argc, char *argv[])
{
	bool decrypt = false;
	bool split = false;
	char *output_file = NULL;
	char *input_encoding = "CP932";
	char *output_encoding = "UTF-8";

	while (1) {
		static struct option long_options[] = {
			{ "help",    no_argument,       0, LOPT_HELP },
			{ "decrypt", no_argument,       0, LOPT_DECRYPT },
			{ "output",  required_argument, 0, LOPT_OUTPUT },
			{ "split",   no_argument,       0, LOPT_SPLIT },
			{ "input-encoding",  required_argument, 0, LOPT_INPUT_ENCODING },
			{ "output-encoding", required_argument, 0, LOPT_OUTPUT_ENCODING },
		};
		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "hdo:s", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case LOPT_HELP:
			usage();
			return 0;
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

	FILE *out = stdin;
	if (output_file)
		out = fopen(output_file, "wb");
	if (!out)
		ERROR("fopen failed: %s", strerror(errno));

	if ((output_conv = iconv_open(output_encoding, input_encoding)) == (iconv_t)-1) {
		ERROR("iconv_open: %s", strerror(errno));
	}
	if ((utf8_conv = iconv_open("UTF-8", input_encoding)) == (iconv_t)-1) {
		ERROR("iconv_open: %s", strerror(errno));
	}

	if (decrypt) {
		size_t size;
		uint8_t *buf = ex_decrypt(argv[0], &size, NULL);
		if (fwrite(buf, size, 1, out) != 1)
			ERROR("fwrite failed: %s", strerror(errno));

		free(buf);
		return 0;
	}

	struct ex *ex = ex_read(argv[0]);
	if (split) {
		const char *dir;
		if (output_file)
			dir = dirname(output_file);
		else
			dir = ".";
		ex_dump_split(out, ex, dir);
	} else {
		ex_dump(out, ex);
		fclose(out);
	}
	ex_free(ex);

	return 0;
}
