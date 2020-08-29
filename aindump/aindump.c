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
#include <iconv.h>
#include "aindump.h"
#include "little_endian.h"
#include "system4.h"
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"

iconv_t output_conv;
iconv_t utf8_conv;

char *convert_text(iconv_t cd, const char *str);

// encode text in output encoding
char *encode_text_output(const char *str)
{
	return convert_text(output_conv, str);
}

// encode text as UTF-8
char *encode_text_utf8(const char *str)
{
	return convert_text(utf8_conv, str);
}

int32_t code_reader_get_arg(struct code_reader *r, int n)
{
	if (n < 0 || n >= r->instr->nr_args)
		ERROR("Invalid argument number for '%s': %d", r->instr->name, n);
	return LittleEndian_getDW(r->ain->code, r->addr + 2 + n*4);
}

static void for_each_instruction_default_error(possibly_unused struct code_reader *r, char *msg)
{
	ERROR("%s", msg);
}

void for_each_instruction(struct ain *ain, void(*fun)(struct code_reader*, void*), void(*err)(struct code_reader*, char*), void *data)
{
	struct code_reader r = { .ain = ain };

	if (!err)
		err = for_each_instruction_default_error;

	for (r.addr = 0; r.addr < ain->code_size;) {
		uint16_t opcode = LittleEndian_getW(ain->code, r.addr);
		if (opcode >= NR_OPCODES) {
			char buf[512];
			snprintf(buf, 512, "Unknown/invalid opcode: %u", opcode);
			err(&r, buf);
			return;
		}

		r.instr = &instructions[opcode];
		if (r.addr + r.instr->nr_args * 4 >= ain->code_size) {
			err(&r, "CODE section truncated?");
			return;
		}

		fun(&r, data);

		r.addr += instruction_width(r.instr->opcode);
		r.instr = NULL;
	}
}

static void usage(void)
{
	puts("Usage: aindump <options> <ainfile>");
	puts("    Display information from AIN files.");
	puts("");
	puts("Commonly used options:");
	puts("    -h, --help                     Display this message and exit");
	puts("    -c, --code                     Dump code section");
	puts("    -j, --json                     Dump to JSON format");
	puts("    -t, --text                     Dump strings and messages, sorted by function");
	puts("    -o, --output                   Set output file path");
	puts("");
	puts("Less used options:");
	puts("    -C, --raw-code                 Dump code section (raw)");
	puts("    -f, --functions                Dump functions section");
	puts("    -g, --globals                  Dump globals section");
	puts("    -S, --structures               Dump structures section");
	puts("    -m, --messages                 Dump messages section");
	puts("    -s, --strings                  Dump strings section");
	puts("    -l, --libraries                Dump libraries section");
	puts("    -F, --filenames                Dump filenames");
	puts("        --function-types           Dump function types section");
	puts("        --delegates                Dump delegate types section");
	puts("        --global-group-names       Dump global group names section");
	puts("    -e, --enums                    Dump enums section");
	puts("        --keycode                  Dump keycode value");
	puts("        --main                     Dump main function index");
	puts("        --msgf                     Dump message function index");
	puts("        --game-version             Dump game version");
	puts("        --ojmp                     Dump OJMP value");
	puts("    -d, --decrypt                  Dump decrypted AIN file");
	puts("        --map                      Dump AIN file map");
	puts("        --no-macros                Don't use macros in code output");
	puts("        --input-encoding <enc>     Specify the text encoding of the AIN file (default: CP932)");
	puts("        --output-encoding <enc>    Specify the text encoding of the output file (default: UTF-8)");
}

static void print_sjis(FILE *f, const char *s)
{
	char *u = encode_text_output(s);
	fprintf(f, "%s", u);
	free(u);
}

static void print_type(FILE *f, struct ain *ain, struct ain_type *t)
{
	char *str = ain_strtype_d(ain, t);
	print_sjis(f, str);
	free(str);
}

static void ain_dump_version(FILE *f, struct ain *ain)
{
	fprintf(f, "AIN VERSION %d\n", ain->version);
}

static void print_arglist(FILE *f, struct ain *ain, struct ain_variable *args, int nr_args)
{
	if (!nr_args) {
		fprintf(f, "(void)");
		return;
	}
	fputc('(', f);
	for (int i = 0; i < nr_args; i++) {
		if (args[i].type.data == AIN_VOID)
			continue;
		if (i > 0)
			fprintf(f, ", ");
		print_sjis(f, ain_variable_to_string(ain, &args[i]));
	}
	fputc(')', f);
}

static void print_varlist(FILE *f, struct ain *ain, struct ain_variable *vars, int nr_vars)
{
	for (int i = 0; i < nr_vars; i++) {
		if (i > 0)
			fputc(',', f);
		fputc(' ', f);
		print_sjis(f, ain_variable_to_string(ain, &vars[i]));
	}
}

void ain_dump_function(FILE *out, struct ain *ain, struct ain_function *f)
{
	print_type(out, ain, &f->return_type);
	fputc(' ', out);
	print_sjis(out, f->name);
	print_arglist(out, ain, f->vars, f->nr_args);
	print_varlist(out, ain, f->vars+f->nr_args, f->nr_vars - f->nr_args);
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
		struct ain_variable *g = &ain->globals[i];
		if (g->type.data == AIN_VOID)
			continue;
		print_sjis(f, ain_variable_to_string(ain, g));
		fprintf(f, ";\n");
	}
}

static void print_structure(FILE *f, struct ain *ain, struct ain_struct *s)
{
	fprintf(f, "struct ");
	print_sjis(f, s->name);

	if (s->nr_interfaces) {
		fprintf(f, " implements");
		for (int i = 0; i < s->nr_interfaces; i++) {
			if (i > 0)
				fputc(',', f);
			fputc(' ', f);
			print_sjis(f, ain->structures[s->interfaces[i].struct_type].name);
		}
	}

	fprintf(f, " {\n");
	for (int i = 0; i < s->nr_members; i++) {
		struct ain_variable *m = &s->members[i];
		if (m->type.data == AIN_VOID)
			continue;
		fprintf(f, "    ");
		print_sjis(f, ain_variable_to_string(ain, m));
		fprintf(f, ";\n");
	}
	fprintf(f, "};\n\n");
}

static void ain_dump_structures(FILE *f, struct ain *ain)
{
	for (int i = 0; i < ain->nr_structures; i++) {
		fprintf(f, "// %d\n", i);
		print_structure(f, ain, &ain->structures[i]);
	}
}

struct dump_text_data {
	FILE *out;
	struct ain_function *fun;
};

static void dump_text_function(struct dump_text_data *data)
{
	if (!data->fun)
		return;

	char *u = encode_text_output(data->fun->name);
	fprintf(data->out, "\n; %s\n", u);
	free(u);

	data->fun = NULL;
}

static void dump_text_string(struct dump_text_data *data, struct ain *ain, int no)
{
	if (no < 0 || no >= ain->nr_strings)
		ERROR("Invalid string index: %d", no);

	// skip empty string
	if (!ain->strings[no]->size)
		return;

	dump_text_function(data);

	char *u = escape_string(ain->strings[no]->text);
	fprintf(data->out, ";s[%d] = \"%s\"\n", no, u);
	free(u);
}

static void dump_text_message(struct dump_text_data *data, struct ain *ain, int no)
{
	dump_text_function(data);

	if (no < 0 || no >= ain->nr_messages)
		ERROR("Invalid message index: %d", no);

	char *u = escape_string(ain->messages[no]->text);
	fprintf(data->out, ";m[%d] = \"%s\"\n", no, u);
	free(u);
}

static void dump_text_instruction(struct code_reader *r, void *_data)
{
	int n;
	struct dump_text_data *data = _data;
	switch (r->instr->opcode) {
	case FUNC:
		n = code_reader_get_arg(r, 0);
		if (n < 0 || n >= r->ain->nr_functions)
			ERROR("Invalid function index: %d", n);
		data->fun = &r->ain->functions[n];
		break;
	case S_PUSH:
		dump_text_string(data, r->ain, code_reader_get_arg(r, 0));
		break;
	// TODO: other instructions with string arguments
	case MSG:
		dump_text_message(data, r->ain, code_reader_get_arg(r, 0));
		break;
	default:
		break;
	}
}

static void ain_dump_text(FILE *f, struct ain *ain)
{
	struct dump_text_data data = {
		.out = f,
		.fun = NULL
	};
	for_each_instruction(ain, dump_text_instruction, NULL, &data);
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
		for (int j = 0; j < ain->libraries[i].nr_functions; j++) {
			struct ain_hll_function *f = &ain->libraries[i].functions[j];
			print_sjis(out, ain_strtype(ain, f->return_type.data, f->return_type.struc));
			fputc(' ', out);
			print_sjis(out, f->name);
			fputc('(', out);
			for (int k = 0; k < f->nr_arguments; k++) {
				struct ain_hll_argument *a = &f->arguments[k];
				if (a->type.data == AIN_VOID)
					continue;
				if (k > 0)
					fprintf(out, ", ");
				print_sjis(out, ain_strtype(ain, a->type.data, a->type.struc));
				fputc(' ', out);
				print_sjis(out, a->name);
			}
			fprintf(out, ")\n");
		}
	}
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
		guess_filenames(ain);

	for (int i = 0; i < ain->nr_filenames; i++) {
		fprintf(f, "0x%08x:\t", i);
		print_sjis(f, ain->filenames[i]);
		fputc('\n', f);
	}
}

static void ain_dump_functypes(FILE *f, struct ain *ain, bool delegates)
{
	struct ain_function_type *types = delegates ? ain->delegates : ain->function_types;
	int n = delegates ? ain->nr_delegates : ain->nr_function_types;
	for (int i = 0; i < n; i++) {
		fprintf(f, "/* 0x%08x */\t", i);
		struct ain_function_type *t = &types[i];
		fprintf(f, delegates ? "delegate " : "functype ");

		print_type(f, ain, &t->return_type);
		fputc(' ', f);
		print_sjis(f, t->name);
		print_arglist(f, ain, t->variables, t->nr_arguments);
		print_varlist(f, ain, t->variables+t->nr_arguments, t->nr_variables-t->nr_arguments);
		fputc('\n', f);
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
		fprintf(f, "// %d\nenum ", i);
		print_sjis(f, ain->enums[i].name);
		fprintf(f, " {");
		for (int j = 0; j < ain->enums[i].nr_symbols; j++) {
			if (j)
				fputc(',', f);
			fprintf(f, "\n\t");
			print_sjis(f, ain->enums[i].symbols[j]);
		}
		fprintf(f, "\n};\n\n");
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
	LOPT_HELP = 256,
	LOPT_AIN_VERSION,
	LOPT_CODE,
	LOPT_RAW_CODE,
	LOPT_JSON,
	LOPT_TEXT,
	LOPT_OUTPUT,
	LOPT_FUNCTIONS,
	LOPT_GLOBALS,
	LOPT_STRUCTURES,
	LOPT_MESSAGES,
	LOPT_STRINGS,
	LOPT_LIBRARIES,
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
	LOPT_DECRYPT,
	LOPT_MAP,
	LOPT_NO_MACROS,
	LOPT_INPUT_ENCODING,
	LOPT_OUTPUT_ENCODING
};

int main(int argc, char *argv[])
{
	initialize_instructions();
	bool decrypt = false;
	char *output_file = NULL;
	FILE *output = stdout;
	char *input_encoding = "CP932";
	char *output_encoding = "UTF-8";
	int err = AIN_SUCCESS;
	unsigned int flags = 0;
	struct ain *ain;

	int dump_targets[256];
	int dump_ptr = 0;

	while (1) {
		static struct option long_options[] = {
			{ "help",               no_argument,       0, LOPT_HELP },
			{ "code",               no_argument,       0, LOPT_CODE },
			{ "raw-code",           no_argument,       0, LOPT_RAW_CODE },
			{ "json",               no_argument,       0, LOPT_JSON },
			{ "text",               no_argument,       0, LOPT_TEXT },
			{ "output",             required_argument, 0, LOPT_OUTPUT },
			{ "ain-version",        no_argument,       0, LOPT_AIN_VERSION },
			{ "functions",          no_argument,       0, LOPT_FUNCTIONS },
			{ "globals",            no_argument,       0, LOPT_GLOBALS },
			{ "structures",         no_argument,       0, LOPT_STRUCTURES },
			{ "messages",           no_argument,       0, LOPT_MESSAGES },
			{ "strings",            no_argument,       0, LOPT_STRINGS },
			{ "libraries",          no_argument,       0, LOPT_LIBRARIES },
			{ "filenames",          no_argument,       0, LOPT_FILENAMES },
			{ "function-types",     no_argument,       0, LOPT_FUNCTION_TYPES },
			{ "delegates",          no_argument,       0, LOPT_DELEGATES },
			{ "global-group-names", no_argument,       0, LOPT_GLOBAL_GROUPS },
			{ "enums",              no_argument,       0, LOPT_ENUMS },
			{ "keycode",            no_argument,       0, LOPT_KEYCODE },
			{ "main",               no_argument,       0, LOPT_MAIN },
			{ "msgf",               no_argument,       0, LOPT_MSGF },
			{ "game-version",       no_argument,       0, LOPT_GAME_VERSION },
			{ "ojmp",               no_argument,       0, LOPT_OJMP },
			{ "decrypt",            no_argument,       0, LOPT_DECRYPT },
			{ "map",                no_argument,       0, LOPT_MAP },
			{ "no-macros",          no_argument,       0, LOPT_NO_MACROS },
			{ "input-encoding",     required_argument, 0, LOPT_INPUT_ENCODING },
			{ "output-encoding",    required_argument, 0, LOPT_OUTPUT_ENCODING },
		};
		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "hcCjto:VfgSmslFeAd", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case LOPT_HELP:
			usage();
			return 0;
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
			output_file = xstrdup(optarg);
			break;
		case 'V':
		case LOPT_AIN_VERSION:
			dump_targets[dump_ptr++] = LOPT_AIN_VERSION;
			break;
		case 'f':
		case LOPT_FUNCTIONS:
			dump_targets[dump_ptr++] = LOPT_FUNCTIONS;
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
		case LOPT_INPUT_ENCODING:
			input_encoding = optarg;
			break;
		case LOPT_OUTPUT_ENCODING:
			output_encoding = optarg;
			break;
		case '?':
			ERROR("Unkown command line argument");
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		ERROR("Wrong number of arguments.\n");
		return 1;
	}

	if (output_file) {
		if (!(output = fopen(output_file, "w"))) {
			ERROR("Failed to open output file '%s': %s", output_file, strerror(errno));
		}
		free(output_file);
	}

	if ((output_conv = iconv_open(output_encoding, input_encoding)) == (iconv_t)-1) {
		ERROR("iconv_open: %s", strerror(errno));
	}
	if ((utf8_conv = iconv_open("UTF-8", input_encoding)) == (iconv_t)-1) {
		ERROR("iconv_open: %s", strerror(errno));
	}

	if (decrypt) {
		dump_decrypted(output, argv[0]);
		return 0;
	}

	if (!(ain = ain_open(argv[0], &err))) {
		ERROR("Failed to open ain file: %s\n", ain_strerror(err));
		return 1;
	}
	ain_init_member_functions(ain, encode_text_utf8);

	for (int i = 0; i < dump_ptr; i++) {
		switch (dump_targets[i]) {
		case LOPT_CODE:           disassemble_ain(output, ain, flags); break;
		case LOPT_JSON:           ain_dump_json(output, ain); break;
		case LOPT_TEXT:           ain_dump_text(output, ain); break;
		case LOPT_AIN_VERSION:    ain_dump_version(output, ain); break;
		case LOPT_FUNCTIONS:      ain_dump_functions(output, ain); break;
		case LOPT_GLOBALS:        ain_dump_globals(output, ain); break;
		case LOPT_STRUCTURES:     ain_dump_structures(output, ain); break;
		case LOPT_MESSAGES:       ain_dump_messages(output, ain); break;
		case LOPT_STRINGS:        ain_dump_strings(output, ain); break;
		case LOPT_LIBRARIES:      ain_dump_libraries(output, ain); break;
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
		case LOPT_MAP:            ain_dump_map(output, ain); break;
		}
	}

	ain_free(ain);
	return 0;
}
