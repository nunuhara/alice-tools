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
#include <math.h>
#include <getopt.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "../dasm.h"

static void ain_compare(struct ain *a, struct ain *b);

static void usage(void)
{
	puts("Usage: aincmp <ainfile> <ainfile>");
	puts("    Compare two ain files");
}

enum {
	LOPT_HELP = 256,
};

static int exit_code = 0;

int main(int argc, char *argv[])
{
	initialize_instructions();

	int err;
	struct ain *a, *b;

	while (1) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, LOPT_HELP },
		};
		int option_index = 0;
		int c = getopt_long(argc, argv, "h", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case LOPT_HELP:
			usage();
			return 0;
		case '?':
			ERROR("Unexpected command line argument");
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2) {
		usage();
		ERROR("Wrong number of arguments");
		return 1;
	}

	if (!(a = ain_open(argv[0], &err)) || !(b = ain_open(argv[1], &err))) {
		ERROR("Failed to open ain file: %s", ain_strerror(err));
	}

	ain_compare(a, b);
	ain_free(a);
	ain_free(b);
	puts(exit_code ? "AIN files differ" : "AIN files match");
	return exit_code;
}

static void _ain_compare_section(struct ain_section *a, struct ain_section *b, const char *name)
{
	if (a->present != b->present || a->size != b->size) {
		NOTICE("%s section differs", name);
		exit_code = 1;
	}
}
#define ain_compare_section(a, b, name) _ain_compare_section(&a->name , &b->name , #name)

#define FLOAT_TOLERANCE 0.0001

union float_cast {
	int32_t i;
	float f;
};

static float float_cast(int32_t i)
{
	union float_cast u = { .i = i };
	return u.f;
}

static bool float_equal(int32_t a, int32_t b)
{
	return fabsf(float_cast(a) - float_cast(b)) < FLOAT_TOLERANCE;
}

static bool ain_compare_code(struct ain *_a, struct ain *_b)
{
	struct dasm_state a, b;
	dasm_init(&a, NULL, _a, 0);
	dasm_init(&b, NULL, _b, 0);

	for (dasm_reset(&a), dasm_reset(&b); !dasm_eof(&a) && !dasm_eof(&b); dasm_next(&a), dasm_next(&b)) {
		if (a.instr->opcode != b.instr->opcode) {
			NOTICE("opcode differs at 0x%08x (%s vs %s)", (uint32_t)a.addr, a.instr->name, b.instr->name);
			return false;
		}
		for (int i = 0; i < a.instr->nr_args; i++) {
			int32_t ia = dasm_arg(&a, i);
			int32_t ib = dasm_arg(&b, i);
			if (a.instr->args[i] == T_FLOAT) {
				if (!float_equal(ia, ib)) {
					NOTICE("float argument differs at 0x%08x (%f vs %f)", (uint32_t)a.addr,
					       float_cast(ia), float_cast(ib));
					return false;
				}
			} else {
				if (ia != ib) {
					// NOTE: If there's duplicate strings in the string table, string arguments
					//       can change when rebuilding. This shouldn't matter (?).
					if (a.instr->args[i] == T_STRING && strcmp(_a->strings[ia]->text, _b->strings[ib]->text)) {
						NOTICE("string argument differs at 0x%08x (%s vs %s)", (uint32_t)a.addr,
						       _a->strings[ia]->text, _b->strings[ib]->text);
						return false;
					} else if (a.instr->args[i] != T_STRING) {
						NOTICE("argument differs at 0x%08x (%d vs %d)", (uint32_t)a.addr, ia, ib);
						return false;
					}
				}
			}
		}
	}
	return true;
}

static bool type_equal(struct ain_type *a, struct ain_type *b)
{
	if (a->data != b->data || a->struc != b->struc || a->rank != b->rank)
		return false;
	if ((a->array_type && !b->array_type) || (b->array_type && !a->array_type))
		return false;
	if (!a->array_type)
		return true;

	for (int i = 0; i < a->rank; i++) {
		if (!type_equal(&a->array_type[i], &b->array_type[i]))
			return false;
	}

	return true;
}

static bool variable_equal(struct ain_variable *a, struct ain_variable *b)
{
	if (strcmp(a->name, b->name))
		return false;
	if (!a->name2 != !b->name2)
		return false;
	if (a->name2 && strcmp(a->name2, b->name2))
		return false;
	if (!type_equal(&a->type, &b->type))
		return false;
	if (a->has_initval != b->has_initval)
		return false;
	// TODO: compare initvals
	if (a->group_index != b->group_index)
		return false;
	return true;
}

static bool ain_compare_functions(struct ain *a, struct ain *b)
{
	if (a->nr_functions != b->nr_functions) {
		NOTICE("number of functions differs (%d vs %d)", a->nr_functions, b->nr_functions);
		return false;
	}

	for (int i = 0; i < a->nr_functions; i++) {
		struct ain_function *fa = &a->functions[i], *fb = &b->functions[i];
		if (strcmp(fa->name, fb->name)) {
			NOTICE("function name differs (\"%s\" vs \"%s\")", fa->name, fb->name);
			return false;
		}
		if (fa->address != fb->address) {
			// NOTE: address of NULL function doesn't matter
			if (strcmp(fa->name, "NULL")) {
				NOTICE("function address differs for %s (0x%08x vs 0x%08x)", fa->name, fa->address, fb->address);
				return false;
			}
		}
		if (fa->is_label != fb->is_label) {
			NOTICE("function is_label differs for %s (%d vs %d)", fa->name, fa->is_label, fb->is_label);
			return false;
		}

		if (!type_equal(&fa->return_type, &fb->return_type)) {
			NOTICE("function return type differs for %s", fa->name); // TODO: print types
			return false;
		}

		if (fa->nr_args != fb->nr_args) {
			NOTICE("function argument count differs for %s (%d vs %d)", fa->name, fa->nr_args, fb->nr_args);
			return false;
		}

		if (fa->nr_vars != fb->nr_vars) {
			NOTICE("function variable count differs for %s (%d vs %d)", fa->name, fa->nr_vars, fb->nr_vars);
			return false;
		}

		if (fa->is_lambda != fb->is_lambda) {
			NOTICE("function is_lambda differs for %s (%d vs %d)", fa->name, fa->is_lambda, fb->is_lambda);
			return false;
		}

		if (fa->crc != fb->crc) {
			NOTICE("function crc differs for %s (%d vs %d)", fa->name, fa->crc, fb->crc);
			return false;
		}

		for (int j = 0; j < fa->nr_vars; j++) {
			if (!variable_equal(&fa->vars[j], &fb->vars[j])) {
				NOTICE("function variable %d differs for %s", j, fa->name);
			}
		}
	}
	return true;
}

static bool ain_compare_globals(struct ain *a, struct ain *b)
{
	if (a->nr_globals != b->nr_globals) {
		NOTICE("number of globals differs (%d vs %d)", a->nr_globals, b->nr_globals);
		return false;
	}

	for (int i = 0; i < a->nr_globals; i++) {
		if (!variable_equal(&a->globals[i], &b->globals[i])) {
			NOTICE("global variable %d (%s) differs", i, a->globals[i].name);
			return false;
		}
	}
	return true;
}

static bool initval_equal(struct ain_initval *a, struct ain_initval *b)
{
	if (a->global_index != b->global_index)
		return false;
	if (a->data_type != b->data_type)
		return false;
	if (a->data_type == AIN_STRING)
		return !strcmp(a->string_value, b->string_value);
	if (a->data_type == AIN_FLOAT)
		return float_equal(a->int_value, b->int_value);
	return a->int_value == b->int_value;
}

static bool ain_compare_global_initvals(struct ain *a, struct ain *b)
{
	if (a->nr_initvals != b->nr_initvals) {
		NOTICE("number of global initvals differs (%d vs %d)", a->nr_initvals, b->nr_initvals);
		return false;
	}

	for (int i = 0; i < a->nr_initvals; i++) {
		if (!initval_equal(&a->global_initvals[i], &b->global_initvals[i])) {
			NOTICE("global initval %d differs", i);
			return false;
		}
	}
	return true;
}

static bool struct_equal(struct ain_struct *a, struct ain_struct *b)
{
	if (strcmp(a->name, b->name))
		return false;
	if (a->nr_interfaces != b->nr_interfaces)
		return false;
	if (a->constructor != b->constructor)
		return false;
	if (a->destructor != b->destructor)
		return false;
	if (a->nr_members != b->nr_members)
		return false;
	for (int i = 0; i < a->nr_members; i++) {
		if (!variable_equal(&a->members[i], &b->members[i]))
			return false;
	}
	return true;
}

static bool ain_compare_structs(struct ain *a, struct ain *b)
{
	if (a->nr_structures != b->nr_structures) {
		NOTICE("number of structures differs (%d vs %d)", a->nr_structures, b->nr_structures);
		return false;
	}

	for (int i = 0; i < a->nr_structures; i++) {
		if (!struct_equal(&a->structures[i], &b->structures[i])) {
			NOTICE("structure %s differs", a->structures[i].name);
			return false;
		}
	}
	return true;
}

static bool ain_compare_messages(struct ain *a, struct ain *b)
{
	if (a->nr_messages != b->nr_messages) {
		NOTICE("number of messages differs (%d vs %d)", a->nr_messages, b->nr_messages);
		return false;
	}

	for (int i = 0; i < a->nr_messages; i++) {
		if (strcmp(a->messages[i]->text, b->messages[i]->text)) {
			NOTICE("message %d differs", i);
			return false;
		}
	}
	return true;
}

static bool hll_function_equal(struct ain_hll_function *a, struct ain_hll_function *b)
{
	if (strcmp(a->name, b->name))
		return false;
	if (!type_equal(&a->return_type, &b->return_type))
		return false;
	if (a->nr_arguments != b->nr_arguments)
		return false;
	for (int i = 0; i < a->nr_arguments; i++) {
		if (strcmp(a->arguments[i].name, b->arguments[i].name))
			return false;
		if (!type_equal(&a->arguments[i].type, &b->arguments[i].type))
			return false;
	}
	return true;
}

static bool ain_compare_library(struct ain_library *a, struct ain_library *b)
{
	if (strcmp(a->name, b->name)) {
		NOTICE("library name differs (\"%s\" vs \"%s\")", a->name, b->name);
		return false;
	}

	if (a->nr_functions != b->nr_functions) {
		NOTICE("library function count differs for %s (%d vs %d)", a->name, a->nr_functions, b->nr_functions);
		return false;
	}

	for (int i = 0; i < a->nr_functions; i++) {
		if (!hll_function_equal(&a->functions[i], &b->functions[i])) {
			NOTICE("library function %s.%s differs", a->name, a->functions[i].name);
			return false;
		}
	}

	return true;
}

static bool ain_compare_libraries(struct ain *a, struct ain *b)
{
	if (a->nr_libraries != b->nr_libraries) {
		NOTICE("number of libraries differs (%d vs %d)", a->nr_libraries, b->nr_libraries);
		return false;
	}

	for (int i = 0; i < a->nr_libraries; i++) {
		if (!ain_compare_library(&a->libraries[i], &b->libraries[i]))
			return false;
	}
	return true;
}

static bool switch_equal(struct ain_switch *a, struct ain_switch *b)
{
	if (a->case_type != b->case_type)
		return false;
	if (a->default_address != b->default_address)
		return false;
	if (a->nr_cases != b->nr_cases)
		return false;
	for (int i = 0; i < a->nr_cases; i++) {
		if (a->cases[i].value != b->cases[i].value)
			return false;
		if (a->cases[i].address != b->cases[i].address)
			return false;
	}
	return true;
}

static bool ain_compare_switches(struct ain *a, struct ain *b)
{
	if (a->nr_switches != b->nr_switches) {
		NOTICE("number of switches differs (%d vs %d)", a->nr_switches, b->nr_switches);
		return false;
	}

	for (int i = 0; i < a->nr_switches; i++) {
		if (!switch_equal(&a->switches[i], &b->switches[i])) {
			NOTICE("switch %d differs", i);
			return false;
		}
	}
	return true;
}

static bool ain_compare_strings(struct ain *a, struct ain *b)
{
	if (a->nr_strings != b->nr_strings) {
		NOTICE("number of strings differs (%d vs %d)", a->nr_strings, b->nr_strings);
		return false;
	}

	for (int i = 0; i < a->nr_strings; i++) {
		if (strcmp(a->strings[i]->text, b->strings[i]->text)) {
			NOTICE("string %d differs", i);
			return false;
		}
	}
	return true;
}

static bool ain_compare_filenames(struct ain *a, struct ain *b)
{
	if (a->nr_filenames != b->nr_filenames) {
		NOTICE("number of filenames differs (%d vs %d)", a->nr_filenames, b->nr_filenames);
		return false;
	}

	for (int i = 0; i < a->nr_filenames; i++) {
		if (strcmp(a->filenames[i], b->filenames[i])) {
			NOTICE("filename %d differs", i);
			return false;
		}
	}
	return true;
}

static bool function_type_equal(struct ain_function_type *a, struct ain_function_type *b)
{
	if (strcmp(a->name, b->name))
		return false;
	if (!type_equal(&a->return_type, &b->return_type))
		return false;
	if (a->nr_arguments != b->nr_arguments)
		return false;
	if (a->nr_variables != b->nr_variables)
		return false;
	for (int i = 0; i < a->nr_variables; i++) {
		if (!variable_equal(&a->variables[i], &b->variables[i]))
			return false;
	}
	return true;
}

static bool ain_compare_function_types(struct ain *a, struct ain *b)
{
	if (a->nr_function_types != b->nr_function_types) {
		NOTICE("number of function types differs (%d vs %d)", a->nr_function_types, b->nr_function_types);
		return false;
	}
	if (a->nr_delegates != b->nr_delegates) {
		NOTICE("number of delegates differs (%d vs %d)", a->nr_delegates, b->nr_delegates);
		return false;
	}

	int count;
	struct ain_function_type *types_a, *types_b;
	if (a->delegates) {
		types_a = a->delegates;
		types_b = b->delegates;
		count = a->nr_delegates;
	} else {
		types_a = a->function_types;
		types_b = b->function_types;
		count = a->nr_function_types;
	}

	for (int i = 0; i < count; i++) {
		if (!function_type_equal(&types_a[i], &types_b[i])) {
			NOTICE("functype/delegate %d differs", i);
			return false;
		}
	}
	return true;
}

static bool ain_compare_global_groups(struct ain *a, struct ain *b)
{
	if (a->nr_global_groups != b->nr_global_groups) {
		NOTICE("number of global groups differs (%d vs %d)", a->nr_global_groups, b->nr_global_groups);
		return false;
	}

	for (int i = 0; i < a->nr_global_groups; i++) {
		if (strcmp(a->global_group_names[i], b->global_group_names[i])) {
			NOTICE("global group %d differs (\"%s\" vs \"%s\")", i, a->global_group_names[i], b->global_group_names[i]);
			return false;
		}
	}
	return true;
}

static bool ain_compare_enums(struct ain *a, struct ain *b)
{
	if (a->nr_enums != b->nr_enums) {
		NOTICE("number of enums differs (%d vs %d)", a->nr_enums, b->nr_enums);
		return false;
	}

	for (int i = 0; i < a->nr_enums; i++) {
		if (strcmp(a->enums[i].name, b->enums[i].name)) {
			NOTICE("enum %d differs (\"%s\" vs \"%s\")", i, a->enums[i].name, b->enums[i].name);
			return false;
		}
	}
	return true;
}

static void ain_compare(struct ain *a, struct ain *b)
{
	ain_compare_section(a, b, VERS);
	if (a->version != b->version) {
		NOTICE("ain version differs (%d vs %d)", a->version, b->version);
		exit_code = 1;
	}

	ain_compare_section(a, b, KEYC);
	if (a->keycode != b->keycode) {
		NOTICE("keycode differs (%d vs %d)", a->keycode, b->keycode);
		exit_code = 1;
	}

	ain_compare_section(a, b, CODE);
	if (!ain_compare_code(a, b))
		exit_code = 1;

	ain_compare_section(a, b, FUNC);
	if (!ain_compare_functions(a, b))
		exit_code = 1;

	ain_compare_section(a, b, GLOB);
	if (!ain_compare_globals(a, b))
		exit_code = 1;

	ain_compare_section(a, b, GSET);
	if (!ain_compare_global_initvals(a, b))
		exit_code = 1;

	ain_compare_section(a, b, STRT);
	if (!ain_compare_structs(a, b))
		exit_code = 1;

	ain_compare_section(a, b, MSG0);
	ain_compare_section(a, b, MSG1);
	if (!ain_compare_messages(a, b))
		exit_code = 1;

	ain_compare_section(a, b, MAIN);
	if (a->main != b->main) {
		NOTICE("main function differs (%d vs %d)", a->main, b->main);
		exit_code = 1;
	}

	ain_compare_section(a, b, MSGF);
	if (a->msgf != b->msgf) {
		NOTICE("message function differs (%d vs %d)", a->msgf, b->msgf);
		exit_code = 1;
	}

	ain_compare_section(a, b, HLL0);
	if (!ain_compare_libraries(a, b))
		exit_code = 1;

	ain_compare_section(a, b, SWI0);
	if (!ain_compare_switches(a, b))
		exit_code = 1;

	ain_compare_section(a, b, GVER);
	if (a->game_version != b->game_version) {
		NOTICE("game version differs (%d vs %d)", a->game_version, b->game_version);
		exit_code = 1;
	}

	ain_compare_section(a, b, STR0);
	if (!ain_compare_strings(a, b))
		exit_code = 1;

	ain_compare_section(a, b, FNAM);
	if (!ain_compare_filenames(a, b))
		exit_code = 1;

	ain_compare_section(a, b, OJMP);
	if (a->ojmp != b->ojmp) {
		NOTICE("ojmp differs (%d vs %d)", a->ojmp, b->ojmp);
		exit_code = 1;
	}

	ain_compare_section(a, b, FNCT);
	ain_compare_section(a, b, DELG);
	if (!ain_compare_function_types(a, b))
		exit_code = 1;

	ain_compare_section(a, b, OBJG);
	if (!ain_compare_global_groups(a, b))
		exit_code = 1;

	ain_compare_section(a, b, ENUM);
	if (!ain_compare_enums(a, b))
		exit_code = 1;
}
