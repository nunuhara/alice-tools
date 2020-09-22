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
#include "alice.h"
#include "jaf.h"

extern int text_parse(void);

enum {
	LOPT_PROJECT = 256,
	LOPT_CODE,
	LOPT_JAF,
	LOPT_JSON,
	LOPT_TEXT,
	LOPT_TRANSCODE,
	LOPT_OUTPUT,
	LOPT_RAW,
	LOPT_AIN_VERSION,
	LOPT_SILENT,
};

int command_ain_edit(int argc, char *argv[])
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
	int ain_version = 4;
	bool transcode = false;
	uint32_t flags = 0;

	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ain_edit);
		if (c == -1)
			break;

		switch (c) {
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
			set_input_encoding("CP932");
			set_output_encoding(optarg);
			break;
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case LOPT_RAW:
			flags |= ASM_RAW;
			break;
		case LOPT_AIN_VERSION:
			ain_version = atoi(optarg);
			if (ain_version < 4 || ain_version > 12)
				ALICE_ERROR("Invalid AIN version (4-12 supported)");
			break;
		case LOPT_SILENT:
			sys_silent = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1) {
		USAGE_ERROR(&cmd_ain_edit, "Too many arguments.");
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
			ALICE_ERROR("Failed to open ain file: %s", ain_strerror(err));
		}
	}
	ain_init_member_functions(ain, conv_output_utf8);

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

struct command cmd_ain_edit = {
	.name = "edit",
	.usage = "[options...] <input-file>",
	.description = "Edit a .ain file",
	.parent = &cmd_ain,
	.fun = command_ain_edit,
	.options = {
		{ "output",      'o', "Set the output file path",                     required_argument, LOPT_OUTPUT },
		{ "code",        'c', "Update the CODE section (assemble .jam file)", required_argument, LOPT_CODE },
		{ "jaf",         0,   "Update .ain file from .jaf source code",       required_argument, LOPT_JAF },
		{ "json",        'j', "Update .ain file from json data",              required_argument, LOPT_JSON },
		{ "project",     'p', "Build .ain from project file",                 required_argument, LOPT_PROJECT },
		{ "text",        't', "Update strings/messages",                      required_argument, LOPT_TEXT },
		{ "ain-version", 0,   "Specify the .ain version",                     required_argument, LOPT_AIN_VERSION },
		{ "raw",         0,   "Read code in raw mode",                        no_argument,       LOPT_RAW },
		{ "silent",      0,   "Don't write messages to stdout",               no_argument,       LOPT_SILENT },
		{ "transcode",   0,   "Change the .ain file's text encoding",         required_argument, LOPT_TRANSCODE },
		{ 0 }
	}
};
