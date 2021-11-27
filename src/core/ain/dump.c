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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ain.h"

static void print_sjis(FILE *f, const char *s)
{
	char *u = conv_output(s);
	fprintf(f, "%s", u);
	free(u);
}

static void print_type(FILE *f, struct ain *ain, struct ain_type *t)
{
	char *str = ain_strtype_d(ain, t);
	print_sjis(f, str);
	free(str);
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

void ain_dump_global(FILE *out, struct ain *ain, int i)
{
	assert(i >= 0 && i < ain->nr_globals);
	if (ain->globals[i].type.data == AIN_VOID)
		return;
	print_sjis(out, ain_variable_to_string(ain, &ain->globals[i]));
	fprintf(out, ";\n");
}

void ain_dump_structure(FILE *f, struct ain *ain, int i)
{
	assert(i >= 0 && i < ain->nr_structures);
	struct ain_struct *s = &ain->structures[i];

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
		fprintf(f, "    ");
		if (m->type.data == AIN_VOID) {
			fprintf(f, "// ");
		}
		print_sjis(f, ain_variable_to_string(ain, m));
		fprintf(f, ";\n");
	}
	fprintf(f, "};\n");
}

static void dump_text_function(FILE *out, struct ain_function **fun)
{
	if (!*fun)
		return;

	char *u = conv_output((*fun)->name);
	fprintf(out, "\n; %s\n", u);
	free(u);

	*fun = NULL;
}

static void dump_text_string(FILE *out, struct ain_function **fun, struct ain *ain, int no)
{
	if (no < 0 || no >= ain->nr_strings)
		ERROR("Invalid string index: %d", no);

	// skip empty string
	if (!ain->strings[no]->size)
		return;

	dump_text_function(out, fun);

	char *u = escape_string(ain->strings[no]->text);
	fprintf(out, ";s[%d] = \"%s\"\n", no, u);
	free(u);
}

static void dump_text_message(FILE *out, struct ain_function **fun, struct ain *ain, int no)
{
	dump_text_function(out, fun);

	if (no < 0 || no >= ain->nr_messages)
		ERROR("Invalid message index: %d", no);

	char *u = escape_string(ain->messages[no]->text);
	fprintf(out, ";m[%d] = \"%s\"\n", no, u);
	free(u);
}

void ain_dump_text(FILE *f, struct ain *ain)
{
	struct dasm_state dasm;
	dasm_init(&dasm, NULL, ain, 0);

	struct ain_function *fun = NULL;

	for (dasm_reset(&dasm); !dasm_eof(&dasm); dasm_next(&dasm)) {
		switch (dasm.instr->opcode) {
		case FUNC: {
			int32_t n = dasm_arg(&dasm, 0);
			if (n < 0 || n > ain->nr_functions)
				ERROR("Invalid function index: %d", n);
			fun = &ain->functions[n];
			break;
		}
		case S_PUSH:
			dump_text_string(f, &fun, ain, dasm_arg(&dasm, 0));
			break;
			// TODO: other instructions with string arguments
		case MSG:
			dump_text_message(f, &fun, ain, dasm_arg(&dasm, 0));
			break;
		default:
			break;
		}
	}
}

void ain_dump_library(FILE *out, struct ain *ain, int lib)
{
	for (int i = 0; i < ain->libraries[lib].nr_functions; i++) {
		struct ain_hll_function *f = &ain->libraries[lib].functions[i];
		print_sjis(out, ain_strtype(ain, f->return_type.data, f->return_type.struc));
		fputc(' ', out);
		print_sjis(out, f->name);
		fputc('(', out);
		for (int j = 0; j < f->nr_arguments; j++) {
			struct ain_hll_argument *a = &f->arguments[j];
			if (j > 0) {
				fprintf(out, ", ");
			}
			if (a->type.data == AIN_VOID) {
				fprintf(out, "/* void */");
				continue;
			}
			char *type = ain_strtype_d(ain, &a->type);
			print_sjis(out, type);
			fputc(' ', out);
			print_sjis(out, a->name);
			free(type);
		}
		if (!f->nr_arguments) {
			fprintf(out, "void");
		}
		fprintf(out, ");\n");
	}
}

void ain_dump_functype(FILE *out, struct ain *ain, int i, bool delegate)
{
	struct ain_function_type *t;
	if (delegate) {
		assert(i >= 0 && i < ain->nr_delegates);
		t = &ain->delegates[i];
		fprintf(out, "delegate ");
	} else {
		assert(i >= 0 && i < ain->nr_function_types);
		t = &ain->function_types[i];
		fprintf(out, "functype ");
	}

	print_type(out, ain, &t->return_type);
	fputc(' ', out);
	print_sjis(out, t->name);
	print_arglist(out, ain, t->variables, t->nr_arguments);
	print_varlist(out, ain, t->variables + t->nr_arguments, t->nr_variables - t->nr_arguments);
	fputc('\n', out);
}

void ain_dump_enum(FILE *out, struct ain *ain, int i)
{
	assert(i >= 0 && i < ain->nr_enums);
	struct ain_enum *e = &ain->enums[i];
	fprintf(out, "enum ");
	print_sjis(out, e->name);
	fprintf(out, " {");
	for (int i = 0; i < e->nr_symbols; i++) {
		if (i > 0)
			fputc(',', out);
		fprintf(out, "\n\t");
		print_sjis(out, e->symbols[i]);
		fprintf(out, " = %d", i);
	}
	fprintf(out, "\n};\n");
}
