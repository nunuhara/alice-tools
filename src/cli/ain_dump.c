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
#include "alice/port.h"
#include "cli.h"

static void print_sjis(struct port *port, const char *s)
{
	char *u = conv_output(s);
	port_printf(port, "%s", u);
	free(u);
}

static void ain_dump_version(struct port *port, struct ain *ain)
{
	if (ain->minor_version) {
		port_printf(port, "%d.%d\n", ain->version, ain->minor_version);
	} else {
		port_printf(port, "%d\n", ain->version);
	}
}

static void ain_dump_functions(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_functions; i++) {
		port_printf(port, "/* 0x%08x */\t", i);
		ain_dump_function(port, ain, &ain->functions[i]);
		port_printf(port, ";\n");
	}
}

static void ain_dump_globals(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_globals; i++) {
		port_printf(port, "/* 0x%08x */\t", i);
		ain_dump_global(port, ain, i);
	}
}

static void ain_dump_structures(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_structures; i++) {
		port_printf(port, "// %d\n", i);
		ain_dump_structure(port, ain, i);
		port_printf(port, "\n");
	}
}

static void ain_dump_messages(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_messages; i++) {
		print_sjis(port, ain->messages[i]->text);
		port_putc(port, '\n');
	}
}

static void ain_dump_libraries(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_libraries; i++) {
		port_printf(port, "--- ");
		print_sjis(port, ain->libraries[i].name);
		port_printf(port, " ---\n");
		ain_dump_library(port, ain, i);
	}
}

static void ain_dump_hll(struct port *port, struct ain *ain)
{
	port_printf(port, "SystemSource = {\n");
	for (int i = 0; i < ain->nr_libraries; i++) {
		char *name = conv_output(ain->libraries[i].name);
		size_t name_len = strlen(name);
		port_printf(port, "\"%s.hll\", \"%s\",\n", name, name);

		char *file_name = xmalloc(name_len + 5);
		memcpy(file_name, name, name_len);
		memcpy(file_name+name_len, ".hll", 5);

		struct port file_port;
		if (!port_file_open(&file_port, file_name)) {
			ALICE_ERROR("fopen: %s", strerror(errno));
		}
		ain_dump_library(&file_port, ain, i);
		port_close(&file_port);
		free(file_name);
		free(name);
	}
	port_printf(port, "}\n");
}

static void ain_dump_hll_stubs(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_libraries; i++) {
		char *name = conv_output(ain->libraries[i].name);
		size_t name_len = strlen(name);
		port_printf(port, "%s.c\n", name);

		char *file_name = xmalloc(name_len + 3);
		memcpy(file_name, name, name_len);
		memcpy(file_name+name_len, ".c", 3);

		struct port file_port;
		if (!port_file_open(&file_port, file_name)) {
			ALICE_ERROR("fopen: %s", strerror(errno));
		}
		ain_dump_library_stub(&file_port, &ain->libraries[i]);
		port_close(&file_port);
		free(file_name);
		free(name);
	}
}

static void ain_dump_strings(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_strings; i++) {
		port_printf(port, "0x%08x:\t", i);
		print_sjis(port, ain->strings[i]->text);
		port_putc(port, '\n');
	}
}

static void ain_dump_filenames(struct port *port, struct ain *ain)
{
	if (!ain->FNAM.present)
		ain_guess_filenames(ain);

	for (int i = 0; i < ain->nr_filenames; i++) {
		port_printf(port, "0x%08x:\t", i);
		print_sjis(port, ain->filenames[i]);
		port_putc(port, '\n');
	}
}

static void ain_dump_functypes(struct port *port, struct ain *ain, bool delegates)
{
	int n = delegates ? ain->nr_delegates : ain->nr_function_types;
	for (int i = 0; i < n; i++) {
		port_printf(port, "/* 0x%08x */\t", i);
		ain_dump_functype(port, ain, i, delegates);
	}
}

static void ain_dump_global_group_names(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_global_groups; i++) {
		port_printf(port, "0x%08x:\t", i);
		print_sjis(port, ain->global_group_names[i]);
		port_putc(port, '\n');
	}
}

static void ain_dump_enums(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_enums; i++) {
		port_printf(port, "// %d\n", i);
		ain_dump_enum(port, ain, i);
		port_putc(port, '\n');
	}
}

static void ain_dump_keycode(struct port *port, struct ain *ain)
{
	port_printf(port, "KEYCODE: 0x%x\n", ain->keycode);
}

static void ain_dump_main(struct port *port, struct ain *ain)
{
	port_printf(port, "MAIN: 0x%x\n", ain->main);
}

static void ain_dump_msgf(struct port *port, struct ain *ain)
{
	port_printf(port, "MSGF: 0x%x\n", ain->msgf);
}

static void ain_dump_game_version(struct port *port, struct ain *ain)
{
	port_printf(port, "GAME VERSION: 0x%x\n", ain->game_version);
}

static void ain_dump_ojmp(struct port *port, struct ain *ain)
{
	port_printf(port, "OJMP: 0x%x\n", ain->ojmp);
}

static void ain_dump_slbl(struct port *port, struct ain *ain)
{
	for (int i = 0; i < ain->nr_scenario_labels; i++) {
		port_printf(port, "0x%08x:\t", ain->scenario_labels[i].address);
		print_sjis(port, ain->scenario_labels[i].name);
		port_putc(port, '\n');
	}
}

static void print_section(struct port *port, const char *name, struct ain_section *section)
{
	if (section->present)
		port_printf(port, "%s: %08x -> %08x\n", name, section->addr, section->addr + section->size);
}

static void ain_dump_map(struct port *port, struct ain *ain)
{
	print_section(port, "VERS", &ain->VERS);
	print_section(port, "KEYC", &ain->KEYC);
	print_section(port, "CODE", &ain->CODE);
	print_section(port, "FUNC", &ain->FUNC);
	print_section(port, "GLOB", &ain->GLOB);
	print_section(port, "GSET", &ain->GSET);
	print_section(port, "STRT", &ain->STRT);
	print_section(port, "MSG0", &ain->MSG0);
	print_section(port, "MSG1", &ain->MSG1);
	print_section(port, "MAIN", &ain->MAIN);
	print_section(port, "MSGF", &ain->MSGF);
	print_section(port, "HLL0", &ain->HLL0);
	print_section(port, "SWI0", &ain->SWI0);
	print_section(port, "GVER", &ain->GVER);
	print_section(port, "SLBL", &ain->SLBL);
	print_section(port, "STR0", &ain->STR0);
	print_section(port, "FNAM", &ain->FNAM);
	print_section(port, "OJMP", &ain->OJMP);
	print_section(port, "FNCT", &ain->FNCT);
	print_section(port, "DELG", &ain->DELG);
	print_section(port, "OBJG", &ain->OBJG);
	print_section(port, "ENUM", &ain->ENUM);
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
	LOPT_HLL_STUBS,
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
			dump_args[dump_ptr] = optarg;
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
		case LOPT_HLL_STUBS:
			dump_targets[dump_ptr++] = LOPT_HLL_STUBS;
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
	struct port port;
	port_file_init(&port, output);

	if (decrypt) {
		dump_decrypted(output, argv[0]);
		return 0;
	}

	if (!(ain = ain_open(argv[0], &err))) {
		ALICE_ERROR("Failed to open ain file: %s\n", ain_strerror(err));
		return 1;
	}

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
		case LOPT_CODE:           ain_disassemble(&port, ain, flags); break;
		case LOPT_JSON:           ain_dump_json(output, ain); break;
		case LOPT_TEXT:           ain_dump_text(&port, ain); break;
		case LOPT_AIN_VERSION:    ain_dump_version(&port, ain); break;
		case LOPT_FUNCTIONS:      ain_dump_functions(&port, ain); break;
		case LOPT_FUNCTION:       ain_disassemble_function(&port, ain, dump_args[i], flags); break;
		case LOPT_GLOBALS:        ain_dump_globals(&port, ain); break;
		case LOPT_STRUCTURES:     ain_dump_structures(&port, ain); break;
		case LOPT_MESSAGES:       ain_dump_messages(&port, ain); break;
		case LOPT_STRINGS:        ain_dump_strings(&port, ain); break;
		case LOPT_LIBRARIES:      ain_dump_libraries(&port, ain); break;
		case LOPT_HLL:            ain_dump_hll(&port, ain); break;
		case LOPT_HLL_STUBS:      ain_dump_hll_stubs(&port, ain); break;
		case LOPT_FILENAMES:      ain_dump_filenames(&port, ain); break;
		case LOPT_FUNCTION_TYPES: ain_dump_functypes(&port, ain, false); break;
		case LOPT_DELEGATES:      ain_dump_functypes(&port, ain, true); break;
		case LOPT_GLOBAL_GROUPS:  ain_dump_global_group_names(&port, ain); break;
		case LOPT_ENUMS:          ain_dump_enums(&port, ain); break;
		case LOPT_KEYCODE:        ain_dump_keycode(&port, ain); break;
		case LOPT_MAIN:           ain_dump_main(&port, ain); break;
		case LOPT_MSGF:           ain_dump_msgf(&port, ain); break;
		case LOPT_GAME_VERSION:   ain_dump_game_version(&port, ain); break;
		case LOPT_OJMP:           ain_dump_ojmp(&port, ain); break;
		case LOPT_SLBL:           ain_dump_slbl(&port, ain); break;
		case LOPT_MAP:            ain_dump_map(&port, ain); break;
		}
	}

	port_close(&port);
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
		{ "hll-stubs",          0,   "Dump HLL stubs for xsystem4",                   no_argument,       LOPT_HLL_STUBS },
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
