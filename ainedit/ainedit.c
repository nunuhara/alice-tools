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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <iconv.h>
#include "ainedit.h"
#include "system4.h"
#include "system4/ain.h"
#include "jaf.h"

static void usage(void)
{
	puts("Usage: ainedit [options...] input-file");
	puts("    Edit AIN files.");
	puts("");
	puts("    -h, --help                     Display this message and exit");
	puts("    -p, --project <pje-file>       Build AIN from project file");
	puts("    -c, --code <jam-file>          Update the CODE section (assemble .jam file)");
	puts("        --jaf <jaf-file>           Update AIN file from JAF source code");
	puts("    -j, --json <json-file>         Update AIN file from JSON data");
	puts("    -t, --text <text-file>         Update strings/messages");
	puts("        --transcode <enc>          Change the AIN file's text encoding");
	puts("    -o, --output <path>            Set output file path");
	puts("        --raw                      Read code in raw mode");
	puts("        --input-encoding <enc>     Specify the text encoding of the input file(s) (default: UTF-8)");
	puts("        --output-encoding <enc>    Specify the text encoding of the output file (default: CP932)");
	puts("        --ain-encoding <enc>       Specify the text encoding of the input AIN file");
	puts("        --ain-version <version>    Specify the AIN version (when creating a new AIN file)");
	puts("        --silent                   Don't write messages to stdout");
}

extern int text_parse(void);

enum {
	LOPT_HELP = 256,
	LOPT_PROJECT,
	LOPT_CODE,
	LOPT_JAF,
	LOPT_JSON,
	LOPT_TEXT,
	LOPT_TRANSCODE,
	LOPT_OUTPUT,
	LOPT_RAW,
	LOPT_INPUT_ENCODING,
	LOPT_OUTPUT_ENCODING,
	LOPT_AIN_VERSION,
	LOPT_SILENT,
};

iconv_t ain_conv;
iconv_t print_conv;
iconv_t ain_utf8_conv;

char *convert_text(iconv_t cd, const char *str);

// encode text for AIN file
char *encode_text(const char *str)
{
	return convert_text(ain_conv, str);
}

char *encode_text_for_print(const char *str)
{
	return convert_text(print_conv, str);
}

char *encode_ain_to_utf8(const char *str)
{
	return convert_text(ain_utf8_conv, str);
}

int main(int argc, char *argv[])
{
	initialize_instructions();
	struct ain *ain;
	int err = AIN_SUCCESS;
	const char *project_file = NULL;
	const char *code_file = NULL;
	unsigned nr_jaf_files = 0;
	const char **jaf_files = NULL;
	const char *decl_file = NULL;
	const char *text_file = NULL;
	const char *output_file = NULL;
	const char *input_encoding = "UTF-8";
	const char *output_encoding = "CP932";
	int ain_version = 4;
	bool transcode = false;
	uint32_t flags = 0;
	while (1) {
		static struct option long_options[] = {
			{ "help",            no_argument,       0, LOPT_HELP },
			{ "project",         required_argument, 0, LOPT_PROJECT },
			{ "code",            required_argument, 0, LOPT_CODE },
			{ "jaf",             required_argument, 0, LOPT_JAF },
			{ "json",            required_argument, 0, LOPT_JSON },
			{ "text",            required_argument, 0, LOPT_TEXT },
			{ "transcode",       required_argument, 0, LOPT_TRANSCODE },
			{ "output",          required_argument, 0, LOPT_OUTPUT },
			{ "raw",             no_argument,       0, LOPT_RAW },
			{ "input-encoding",  required_argument, 0, LOPT_INPUT_ENCODING },
			{ "output-encoding", required_argument, 0, LOPT_OUTPUT_ENCODING },
			{ "ain-version",     required_argument, 0, LOPT_AIN_VERSION },
			{ "silent",          no_argument,       0, LOPT_SILENT },
		};
		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "hp:c:j:t:o:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case LOPT_HELP:
			usage();
			return 0;
		case 'p':
		case LOPT_PROJECT:
			project_file = optarg;
			break;
		case 'c':
		case LOPT_CODE:
			code_file = optarg;
			break;
		case LOPT_JAF:
			jaf_files = xrealloc_array(jaf_files, nr_jaf_files, nr_jaf_files+1, sizeof(char*));
			jaf_files[nr_jaf_files++] = optarg;
			break;
		case 'j':
		case LOPT_JSON:
			decl_file = optarg;
			break;
		case 't':
		case LOPT_TEXT:
			text_file = optarg;
			break;
		case LOPT_TRANSCODE:
			transcode = true;
			input_encoding = "CP932";
			output_encoding = optarg;
			break;
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case LOPT_RAW:
			flags |= ASM_RAW;
			break;
		case LOPT_INPUT_ENCODING:
			input_encoding = optarg;
			break;
		case LOPT_OUTPUT_ENCODING:
			output_encoding = optarg;
			break;
		case LOPT_AIN_VERSION:
			ain_version = atoi(optarg);
			if (ain_version < 4 || ain_version > 12)
				ERROR("Invalid AIN version (4-12 supported)");
			break;
		case LOPT_SILENT:
			sys_silent = true;
			break;
		case '?':
			ERROR("Unknown command line argument");
		}
	}
	argc -= optind;
	argv += optind;


	if (argc > 1) {
		usage();
		ERROR("Too many arguments.");
	}

	if ((ain_conv = iconv_open(output_encoding, input_encoding)) == (iconv_t)-1) {
		ERROR("iconv_open: %s", strerror(errno));
	}
	if ((print_conv = iconv_open("UTF-8", input_encoding)) == (iconv_t)-1) {
		ERROR("iconv_open: %s", strerror(errno));
	}
	if ((ain_utf8_conv = iconv_open("UTF-8", output_encoding)) == (iconv_t)-1) {
		ERROR("iconv_open: %s", strerror(errno));
	}

	if (project_file) {
		pje_build(project_file, ain_version);
		return 0;
	}

	if (!output_file) {
		output_file = "out.ain";
	}

	if (!argc) {
		ain = ain_new(ain_version);
	} else {
		if (!(ain = ain_open(argv[0], &err))) {
			ERROR("Failed to open ain file: %s", ain_strerror(err));
		}
	}
	ain_init_member_functions(ain, encode_ain_to_utf8);

	if (transcode) {
		ain_transcode(ain);
		goto write_ain_file;
	}

	if (decl_file) {
		read_declarations(decl_file, ain);
	}

	if (jaf_files) {
		jaf_build(ain, jaf_files, nr_jaf_files, NULL, 0);
	}

	if (code_file) {
		asm_assemble_jam(code_file, ain, flags);
	}

	if (text_file) {
		read_text(text_file, ain);
	}

write_ain_file:
	NOTICE("Writing AIN file...");
	ain_write(output_file, ain);

	free(jaf_files);
	ain_free(ain);
	return 0;
}
