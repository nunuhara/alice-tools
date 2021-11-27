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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <unistd.h>
#include <iconv.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ain.h"
#include "cli.h"

static void print_sjis(FILE *f, const char *s)
{
	char *u = conv_output(s);
	fprintf(f, "%s", u);
	free(u);
}

static void ain_dump_version(FILE *f, struct ain *ain)
{
	if (ain->minor_version) {
		fprintf(f, "%d.%d\n", ain->version, ain->minor_version);
	} else {
		fprintf(f, "%d\n", ain->version);
	}
}

static void ain_dump_functions(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_functions; i++) {
		fprintf(f, "/* 0x%08x */\t", i);
		ain_dump_function(f, ain, &ain->functions[i]);
		fprintf(f, ";\n");
	}
}

static void ain_dump_globals(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_globals; i++) {
		fprintf(f, "/* 0x%08x */\t", i);
		ain_dump_global(f, ain, i);
	}
}

static void ain_dump_structures(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_structures; i++) {
		fprintf(f, "// %d\n", i);
		ain_dump_structure(f, ain, i);
		fprintf(f, "\n");
	}
}

static void ain_dump_messages(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_messages; i++) {
		print_sjis(f, ain->messages[i]->text);
		fputc('\n', f);
	}
}

static void ain_dump_libraries(FILE *out, struct ain *ain)
{
	for (int i = 0; i < ain->nr_libraries; i++) {
		fprintf(out, "--- ");
		print_sjis(out, ain->libraries[i].name);
		fprintf(out, " ---\n");
		ain_dump_library(out, ain, i);
	}
}

static void ain_dump_hll(FILE *out, struct ain *ain)
{
	fprintf(out, "SystemSource = {\n");
	for (int i = 0; i < ain->nr_libraries; i++) {
		char *name = conv_output(ain->libraries[i].name);
		size_t name_len = strlen(name);
		fprintf(out, "\"%s.hll\", \"%s\",\n", name, name);

		char *file_name = xmalloc(name_len + 5);
		memcpy(file_name, name, name_len);
		memcpy(file_name+name_len, ".hll", 5);

		FILE *f = fopen(file_name, "wb");
		if (!f) {
			ALICE_ERROR("fopen: %s", strerror(errno));
		}
		ain_dump_library(f, ain, i);
		fclose(f);
		free(file_name);
		free(name);
	}
	fprintf(out, "}\n");
}

static void ain_dump_strings(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_strings; i++) {
		fprintf(f, "0x%08x:\t", i);
		print_sjis(f, ain->strings[i]->text);
		fputc('\n', f);
	}
}

static void ain_dump_filenames(FILE *f, struct ain *ain)
{
	if (!ain->FNAM.present)
		ain_guess_filenames(ain);

	for (int i = 0; i < ain->nr_filenames; i++) {
		fprintf(f, "0x%08x:\t", i);
		print_sjis(f, ain->filenames[i]);
		fputc('\n', f);
	}
}

static void ain_dump_functypes(FILE *f, struct ain *ain, bool delegates)
{
	int n = delegates ? ain->nr_delegates : ain->nr_function_types;
	for (int i = 0; i < n; i++) {
		fprintf(f, "/* 0x%08x */\t", i);
		ain_dump_functype(f, ain, i, delegates);
	}
}

static void ain_dump_global_group_names(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_global_groups; i++) {
		fprintf(f, "0x%08x:\t", i);
		print_sjis(f, ain->global_group_names[i]);
		fputc('\n', f);
	}
}

static void ain_dump_enums(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_enums; i++) {
		fprintf(f, "// %d\n", i);
		ain_dump_enum(f, ain, i);
		fputc('\n', f);
	}
}

static void ain_dump_keycode(FILE *f, struct ain *ain)
{
	fprintf(f, "KEYCODE: 0x%x\n", ain->keycode);
}

static void ain_dump_main(FILE *f, struct ain *ain)
{
	fprintf(f, "MAIN: 0x%x\n", ain->main);
}

static void ain_dump_msgf(FILE *f, struct ain *ain)
{
	fprintf(f, "MSGF: 0x%x\n", ain->msgf);
}

static void ain_dump_game_version(FILE *f, struct ain *ain)
{
	fprintf(f, "GAME VERSION: 0x%x\n", ain->game_version);
}

static void ain_dump_ojmp(FILE *f, struct ain *ain)
{
	fprintf(f, "OJMP: 0x%x\n", ain->ojmp);
}

static void ain_dump_slbl(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_scenario_labels; i++) {
		fprintf(f, "0x%08x:\t", ain->scenario_labels[i].address);
		print_sjis(f, ain->scenario_labels[i].name);
		fputc('\n', f);
	}
}

static void print_section(FILE *f, const char *name, struct ain_section *section)
{
	if (section->present)
		fprintf(f, "%s: %08x -> %08x\n", name, section->addr, section->addr + section->size);
}

static void ain_dump_map(FILE *f, struct ain *ain)
{
	print_section(f, "VERS", &ain->VERS);
	print_section(f, "KEYC", &ain->KEYC);
	print_section(f, "CODE", &ain->CODE);
	print_section(f, "FUNC", &ain->FUNC);
	print_section(f, "GLOB", &ain->GLOB);
	print_section(f, "GSET", &ain->GSET);
	print_section(f, "STRT", &ain->STRT);
	print_section(f, "MSG0", &ain->MSG0);
	print_section(f, "MSG1", &ain->MSG1);
	print_section(f, "MAIN", &ain->MAIN);
	print_section(f, "MSGF", &ain->MSGF);
	print_section(f, "HLL0", &ain->HLL0);
	print_section(f, "SWI0", &ain->SWI0);
	print_section(f, "GVER", &ain->GVER);
	print_section(f, "SLBL", &ain->SLBL);
	print_section(f, "STR0", &ain->STR0);
	print_section(f, "FNAM", &ain->FNAM);
	print_section(f, "OJMP", &ain->OJMP);
	print_section(f, "FNCT", &ain->FNCT);
	print_section(f, "DELG", &ain->DELG);
	print_section(f, "OBJG", &ain->OBJG);
	print_section(f, "ENUM", &ain->ENUM);
}

static void dump_decrypted(FILE *f, const char *path)
{
	int err;
	long len;
	uint8_t *ain;

	if (!(ain = ain_read(path, &len, &err))) {
		ERROR("Failed to open ain file: %s\n", ain_strerror(err));
	}

	fwrite(ain, len, 1, f);
	fflush(f);
	free(ain);
}

enum {
	LOPT_AIN_VERSION = 256,
	LOPT_CODE,
	LOPT_RAW_CODE,
	LOPT_JSON,
	LOPT_TEXT,
	LOPT_OUTPUT,
	LOPT_FUNCTIONS,
	LOPT_FUNCTION,
	LOPT_GLOBALS,
	LOPT_STRUCTURES,
	LOPT_MESSAGES,
	LOPT_STRINGS,
	LOPT_LIBRARIES,
	LOPT_HLL,
	LOPT_FILENAMES,
	LOPT_FUNCTION_TYPES,
	LOPT_DELEGATES,
	LOPT_GLOBAL_GROUPS,
	LOPT_ENUMS,
	LOPT_KEYCODE,
	LOPT_MAIN,
	LOPT_MSGF,
	LOPT_GAME_VERSION,
	LOPT_OJMP,
	LOPT_SLBL,
	LOPT_DECRYPT,
	LOPT_MAP,
	LOPT_NO_MACROS,
};

int command_ain_dump(int argc, char *argv[])
{
	initialize_instructions();
	set_input_encoding("CP932");
	set_output_encoding("UTF-8");

	bool decrypt = false;
	char *output_file = NULL;
	int err = AIN_SUCCESS;
	unsigned int flags = 0;
	struct ain *ain;

	int dump_targets[256];
	char *dump_args[256];
	int dump_ptr = 0;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ain_dump);
		if (c == -1)
			break;

		switch (c) {
		case 'c':
		case LOPT_CODE:
			dump_targets[dump_ptr++] = LOPT_CODE;
			break;
		case 'C':
		case LOPT_RAW_CODE:
			dump_targets[dump_ptr++] = LOPT_CODE;
			flags |= DASM_RAW;
			break;
		case 'j':
		case LOPT_JSON:
			dump_targets[dump_ptr++] = LOPT_JSON;
			break;
		case 't':
		case LOPT_TEXT:
			dump_targets[dump_ptr++] = LOPT_TEXT;
			break;
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case 'V':
		case LOPT_AIN_VERSION:
			dump_targets[dump_ptr++] = LOPT_AIN_VERSION;
			break;
		case 'f':
		case LOPT_FUNCTIONS:
			dump_targets[dump_ptr++] = LOPT_FUNCTIONS;
			break;
		case LOPT_FUNCTION:
			dump_args[dump_ptr] = conv_cmdline_utf8(optarg);
			dump_targets[dump_ptr++] = LOPT_FUNCTION;
			break;
		case 'g':
		case LOPT_GLOBALS:
			dump_targets[dump_ptr++] = LOPT_GLOBALS;
			break;
		case 'S':
		case LOPT_STRUCTURES:
			dump_targets[dump_ptr++] = LOPT_STRUCTURES;
			break;
		case 'm':
		case LOPT_MESSAGES:
			dump_targets[dump_ptr++] = LOPT_MESSAGES;
			break;
		case 's':
		case LOPT_STRINGS:
			dump_targets[dump_ptr++] = LOPT_STRINGS;
			break;
		case 'l':
		case LOPT_LIBRARIES:
			dump_targets[dump_ptr++] = LOPT_LIBRARIES;
			break;
		case LOPT_HLL:
			dump_targets[dump_ptr++] = LOPT_HLL;
			break;
		case 'F':
		case LOPT_FILENAMES:
			dump_targets[dump_ptr++] = LOPT_FILENAMES;
			break;
		case LOPT_FUNCTION_TYPES:
			dump_targets[dump_ptr++] = LOPT_FUNCTION_TYPES;
			break;
		case LOPT_DELEGATES:
			dump_targets[dump_ptr++] = LOPT_DELEGATES;
			break;
		case LOPT_GLOBAL_GROUPS:
			dump_targets[dump_ptr++] = LOPT_GLOBAL_GROUPS;
			break;
		case 'e':
		case LOPT_ENUMS:
			dump_targets[dump_ptr++] = LOPT_ENUMS;
			break;
		case LOPT_KEYCODE:
			dump_targets[dump_ptr++] = LOPT_KEYCODE;
			break;
		case LOPT_MAIN:
			dump_targets[dump_ptr++] = LOPT_MAIN;
			break;
		case LOPT_MSGF:
			dump_targets[dump_ptr++] = LOPT_MSGF;
			break;
		case LOPT_GAME_VERSION:
			dump_targets[dump_ptr++] = LOPT_GAME_VERSION;
			break;
		case LOPT_OJMP:
			dump_targets[dump_ptr++] = LOPT_OJMP;
			break;
		case LOPT_SLBL:
			dump_targets[dump_ptr++] = LOPT_SLBL;
			break;
		case 'd':
		case LOPT_DECRYPT:
			decrypt = true;
			break;
		case LOPT_MAP:
			dump_targets[dump_ptr++] = LOPT_MAP;
			break;
		case LOPT_NO_MACROS:
			flags |= DASM_NO_MACROS;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		USAGE_ERROR(&cmd_ain_dump, "Wrong number of arguments.\n");
	}

	FILE *output = alice_open_output_file(output_file);

	if (decrypt) {
		dump_decrypted(output, argv[0]);
		return 0;
	}

	char *input_file = conv_cmdline_utf8(argv[0]);
	if (!(ain = ain_open(input_file, &err))) {
		ALICE_ERROR("Failed to open ain file: %s\n", ain_strerror(err));
		return 1;
	}
	free(input_file);

	ain_init_member_functions(ain, conv_utf8);

	// chdir to output file directory so that subsequent opens are relative
	if (output != stdout) {
		char *tmp = strdup(output_file);
		char *dir = dirname(tmp);
		chdir(dir);
		free(tmp);
	}

	for (int i = 0; i < dump_ptr; i++) {
		switch (dump_targets[i]) {
		case LOPT_CODE:           ain_disassemble(output, ain, flags); break;
		case LOPT_JSON:           ain_dump_json(output, ain); break;
		case LOPT_TEXT:           ain_dump_text(output, ain); break;
		case LOPT_AIN_VERSION:    ain_dump_version(output, ain); break;
		case LOPT_FUNCTIONS:      ain_dump_functions(output, ain); break;
		case LOPT_FUNCTION:       ain_disassemble_function(output, ain, dump_args[i], flags); free(dump_args[i]); break;
		case LOPT_GLOBALS:        ain_dump_globals(output, ain); break;
		case LOPT_STRUCTURES:     ain_dump_structures(output, ain); break;
		case LOPT_MESSAGES:       ain_dump_messages(output, ain); break;
		case LOPT_STRINGS:        ain_dump_strings(output, ain); break;
		case LOPT_LIBRARIES:      ain_dump_libraries(output, ain); break;
		case LOPT_HLL:            ain_dump_hll(output, ain); break;
		case LOPT_FILENAMES:      ain_dump_filenames(output, ain); break;
		case LOPT_FUNCTION_TYPES: ain_dump_functypes(output, ain, false); break;
		case LOPT_DELEGATES:      ain_dump_functypes(output, ain, true); break;
		case LOPT_GLOBAL_GROUPS:  ain_dump_global_group_names(output, ain); break;
		case LOPT_ENUMS:          ain_dump_enums(output, ain); break;
		case LOPT_KEYCODE:        ain_dump_keycode(output, ain); break;
		case LOPT_MAIN:           ain_dump_main(output, ain); break;
		case LOPT_MSGF:           ain_dump_msgf(output, ain); break;
		case LOPT_GAME_VERSION:   ain_dump_game_version(output, ain); break;
		case LOPT_OJMP:           ain_dump_ojmp(output, ain); break;
		case LOPT_SLBL:           ain_dump_slbl(output, ain); break;
		case LOPT_MAP:            ain_dump_map(output, ain); break;
		}
	}

	ain_free(ain);
	return 0;
}

struct command cmd_ain_dump = {
	.name = "dump",
	.usage = "[options...] <input-file>",
	.description = "Dump various info from a .ain file",
	.parent = &cmd_ain,
	.fun = command_ain_dump,
	.options = {
		{ "output",             'o', "Set the output file path",                      required_argument, LOPT_OUTPUT },
		{ "code",               'c', "Dump code section",                             no_argument,       LOPT_CODE },
		{ "text",               't', "Dump strings and messages, sorted by function", no_argument,       LOPT_TEXT },
		{ "json",               'j', "Dump to JSON format",                           no_argument,       LOPT_JSON },
		{ "raw-code",           'C', "Dump code section (raw)",                       no_argument,       LOPT_RAW_CODE },
		{ "functions",          'f', "Dump functions section",                        no_argument,       LOPT_FUNCTIONS },
		{ "function",           0,   "Dump function code",                            required_argument, LOPT_FUNCTION },
		{ "globals",            'g', "Dump globals section",                          no_argument,       LOPT_GLOBALS },
		{ "structures",         'S', "Dump structures section",                       no_argument,       LOPT_STRUCTURES },
		{ "messages",           'm', "Dump messages section",                         no_argument,       LOPT_MESSAGES },
		{ "strings",            's', "Dump strings section",                          no_argument,       LOPT_STRINGS },
		{ "libraries",          'l', "Dump libraries section",                        no_argument,       LOPT_LIBRARIES },
		{ "hll",                0,   "Dump HLL files",                                no_argument,       LOPT_HLL },
		{ "filenames",          'F', "Dump filenames",                                no_argument,       LOPT_FILENAMES },
		{ "function-types",     0,   "Dump function types section",                   no_argument,       LOPT_FUNCTION_TYPES },
		{ "delegates",          0,   "Dump delegate types section",                   no_argument,       LOPT_DELEGATES },
		{ "global-group-names", 0,   "Dump global group names section",               no_argument,       LOPT_GLOBAL_GROUPS },
		{ "enums",              'e', "Dump enums section",                            no_argument,       LOPT_ENUMS },
		{ "keycode",            0,   "Dump keycode value",                            no_argument,       LOPT_KEYCODE },
		{ "main",               0,   "Dump main function index",                      no_argument,       LOPT_MAIN },
		{ "msgf",               0,   "Dump message function index",                   no_argument,       LOPT_MSGF },
		{ "ain-version",        0,   "Dump .ain file version",                        no_argument,       LOPT_AIN_VERSION },
		{ "game-version",       0,   "Dump game version",                             no_argument,       LOPT_GAME_VERSION },
		{ "ojmp",               0,   "Dump OJMP value",                               no_argument,       LOPT_OJMP },
		{ "slbl",               0,   "Dump scenario labels section",                  no_argument,       LOPT_SLBL },
		{ "decrypt",            'd', "Dump decrypted .ain file",                      no_argument,       LOPT_DECRYPT },
		{ "map",                0,   "Dump ain file map",                             no_argument,       LOPT_MAP },
		{ "no-macros",          0,   "Don't use macros in code output",               no_argument,       LOPT_NO_MACROS },
		{ 0 }
	}
};
