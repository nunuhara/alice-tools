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
#include <time.h>
#include <assert.h>
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/port.h"

static void print_sjis(struct port *port, const char *s)
{
	char *u = conv_output(s);
	port_printf(port, "%s", u);
	free(u);
}

static void print_type(struct port *port, struct ain *ain, struct ain_type *t)
{
	char *str = ain_strtype_d(ain, t);
	print_sjis(port, str);
	free(str);
}

static void print_arglist(struct port *port, struct ain *ain, struct ain_variable *args, int nr_args)
{
	if (!nr_args) {
		port_printf(port, "(void)");
		return;
	}
	port_putc(port, '(');
	for (int i = 0; i < nr_args; i++) {
		if (args[i].type.data == AIN_VOID)
			continue;
		if (i > 0)
			port_printf(port, ", ");
		print_sjis(port, ain_variable_to_string(ain, &args[i]));
	}
	port_putc(port, ')');
}

static void print_varlist(struct port *port, struct ain *ain, struct ain_variable *vars, int nr_vars)
{
	for (int i = 0; i < nr_vars; i++) {
		if (i > 0)
			port_putc(port, ',');
		port_putc(port, ' ');
		print_sjis(port, ain_variable_to_string(ain, &vars[i]));
	}
}

static void print_xsystem4_license_header(struct port *port)
{
	static const char *author;
	if (!author) {
		author = getenv("AUTHOR");
		if (!author) {
			WARNING("AUTHOR environment variable is not set");
			author = "(INSERT YOUR NAME)";
		}
	}

	time_t t;
	time(&t);
	struct tm *tm = localtime(&t);

	port_printf(
		port,
		"/* Copyright (C) %d %s\n"
		" *\n"
		" * This program is free software; you can redistribute it and/or modify\n"
		" * it under the terms of the GNU General Public License as published by\n"
		" * the Free Software Foundation; either version 2 of the License, or\n"
		" * (at your option) any later version.\n"
		" *\n"
		" * This program is distributed in the hope that it will be useful,\n"
		" * but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		" * GNU General Public License for more details.\n"
		" *\n"
		" * You should have received a copy of the GNU General Public License\n"
		" * along with this program; if not, see <http://gnu.org/licenses/>.\n"
		" */\n"
		"\n",
		tm->tm_year + 1900,
		author);
}

static const char *ain_ctype(enum ain_data_type type)
{
	switch (type) {
	case AIN_VOID:                return "void ";
	case AIN_BOOL:                return "bool ";
	case AIN_INT:                 return "int ";
	case AIN_FLOAT:               return "float ";
	case AIN_STRING:              return "struct string *";
	case AIN_STRUCT:
	case AIN_ARRAY_BOOL:
	case AIN_ARRAY_INT:
	case AIN_ARRAY_FLOAT:
	case AIN_ARRAY_STRING:
	case AIN_ARRAY_STRUCT:        return "struct page *";
	case AIN_REF_BOOL:            return "bool *";
	case AIN_REF_INT:             return "int *";
	case AIN_REF_FLOAT:           return "float *";
	case AIN_REF_STRING:          return "struct string **";
	case AIN_REF_STRUCT:
	case AIN_REF_ARRAY_BOOL:
	case AIN_REF_ARRAY_INT:
	case AIN_REF_ARRAY_FLOAT:
	case AIN_REF_ARRAY_STRING:
	case AIN_REF_ARRAY_STRUCT:    return "struct page **";
	case AIN_IMAIN_SYSTEM:        return "void *";

	default:
		ALICE_ERROR("Unsupported type: %d", type);
	}
}

void ain_dump_function(struct port *port, struct ain *ain, struct ain_function *f)
{
	print_type(port, ain, &f->return_type);
	port_putc(port, ' ');
	print_sjis(port, f->name);
	print_arglist(port, ain, f->vars, f->nr_args);
	print_varlist(port, ain, f->vars+f->nr_args, f->nr_vars - f->nr_args);
}

void ain_dump_global(struct port *port, struct ain *ain, int i)
{
	assert(i >= 0 && i < ain->nr_globals);
	if (ain->globals[i].type.data == AIN_VOID)
		return;
	print_sjis(port, ain_variable_to_string(ain, &ain->globals[i]));
	port_printf(port, ";\n");
}

void ain_dump_structure(struct port *port, struct ain *ain, int i)
{
	assert(i >= 0 && i < ain->nr_structures);
	struct ain_struct *s = &ain->structures[i];

	port_printf(port, "struct ");
	print_sjis(port, s->name);

	if (s->nr_interfaces) {
		port_printf(port, " implements");
		for (int i = 0; i < s->nr_interfaces; i++) {
			if (i > 0)
				port_putc(port, ',');
			port_putc(port, ' ');
			print_sjis(port, ain->structures[s->interfaces[i].struct_type].name);
		}
	}

	port_printf(port, " {\n");
	for (int i = 0; i < s->nr_members; i++) {
		struct ain_variable *m = &s->members[i];
		port_printf(port, "    ");
		if (m->type.data == AIN_VOID) {
			port_printf(port, "// ");
		}
		print_sjis(port, ain_variable_to_string(ain, m));
		port_printf(port, ";\n");
	}
	port_printf(port, "};\n");
}

static void dump_text_function(struct port *port, struct ain_function **fun)
{
	if (!*fun)
		return;

	char *u = conv_output((*fun)->name);
	port_printf(port, "\n; %s\n", u);
	free(u);

	*fun = NULL;
}

static void dump_text_string(struct port *port, struct ain_function **fun, struct ain *ain, int no)
{
	if (no < 0 || no >= ain->nr_strings)
		ERROR("Invalid string index: %d", no);

	// skip empty string
	if (!ain->strings[no]->size)
		return;

	dump_text_function(port, fun);

	char *u = escape_string(ain->strings[no]->text);
	port_printf(port, ";s[%d] = \"%s\"\n", no, u);
	free(u);
}

static void dump_text_message(struct port *port, struct ain_function **fun, struct ain *ain, int no)
{
	dump_text_function(port, fun);

	if (no < 0 || no >= ain->nr_messages)
		ERROR("Invalid message index: %d", no);

	char *u = escape_string(ain->messages[no]->text);
	port_printf(port, ";m[%d] = \"%s\"\n", no, u);
	free(u);
}

void ain_dump_text(struct port *port, struct ain *ain)
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
			dump_text_string(port, &fun, ain, dasm_arg(&dasm, 0));
			break;
			// TODO: other instructions with string arguments
		case _MSG:
			dump_text_message(port, &fun, ain, dasm_arg(&dasm, 0));
			break;
		default:
			break;
		}
	}
}

void ain_dump_library(struct port *port, struct ain *ain, int lib)
{
	for (int i = 0; i < ain->libraries[lib].nr_functions; i++) {
		struct ain_hll_function *f = &ain->libraries[lib].functions[i];
		print_sjis(port, ain_strtype(ain, f->return_type.data, f->return_type.struc));
		port_putc(port, ' ');
		print_sjis(port, f->name);
		port_putc(port, '(');
		for (int j = 0; j < f->nr_arguments; j++) {
			struct ain_hll_argument *a = &f->arguments[j];
			if (j > 0) {
				port_printf(port, ", ");
			}
			if (a->type.data == AIN_VOID) {
				port_printf(port, "/* void */");
				continue;
			}
			char *type = ain_strtype_d(ain, &a->type);
			print_sjis(port, type);
			port_putc(port, ' ');
			print_sjis(port, a->name);
			free(type);
		}
		if (!f->nr_arguments) {
			port_printf(port, "void");
		}
		port_printf(port, ");\n");
	}
}

void ain_dump_library_stub(struct port *port, struct ain_library *lib)
{
	print_xsystem4_license_header(port);
	port_printf(port, "#include \"hll.h\"\n\n");

	char *lib_name = conv_output(lib->name);

	for (int i = 0; i < lib->nr_functions; i++) {
		struct ain_hll_function *f = &lib->functions[i];
		port_printf(port, "//%s%s_", ain_ctype(f->return_type.data), lib_name);
		print_sjis(port, f->name);
		port_putc(port, '(');
		for (int j = 0; j < f->nr_arguments; j++) {
			struct ain_hll_argument *a = &f->arguments[j];
			if (j > 0) {
				port_printf(port, ", ");
			}
			if (a->type.data == AIN_VOID) {
				port_printf(port, "/* void */");
				continue;
			}
			port_printf(port, "%s", ain_ctype(a->type.data));
			print_sjis(port, a->name);
		}
		if (!f->nr_arguments) {
			port_printf(port, "void");
		}
		port_printf(port, ");\n");
	}

	port_printf(port, "\nHLL_LIBRARY(%s", lib_name);

	for (int i = 0; i < lib->nr_functions; i++) {
		struct ain_hll_function *f = &lib->functions[i];
		port_printf(port, ",\n\t    HLL_TODO_EXPORT(");
		print_sjis(port, f->name);
		port_printf(port, ", %s_", lib_name);
		print_sjis(port, f->name);
		port_printf(port, ")");
	}
	port_printf(port, "\n\t    );\n");

	free(lib_name);
}

void ain_dump_functype(struct port *port, struct ain *ain, int i, bool delegate)
{
	struct ain_function_type *t;
	if (delegate) {
		assert(i >= 0 && i < ain->nr_delegates);
		t = &ain->delegates[i];
		port_printf(port, "delegate ");
	} else {
		assert(i >= 0 && i < ain->nr_function_types);
		t = &ain->function_types[i];
		port_printf(port, "functype ");
	}

	print_type(port, ain, &t->return_type);
	port_putc(port, ' ');
	print_sjis(port, t->name);
	print_arglist(port, ain, t->variables, t->nr_arguments);
	print_varlist(port, ain, t->variables + t->nr_arguments, t->nr_variables - t->nr_arguments);
	port_putc(port, '\n');
}

void ain_dump_enum(struct port *port, struct ain *ain, int i)
{
	assert(i >= 0 && i < ain->nr_enums);
	struct ain_enum *e = &ain->enums[i];
	port_printf(port, "enum ");
	print_sjis(port, e->name);
	port_printf(port, " {");
	for (int i = 0; i < e->nr_values; i++) {
		if (i > 0)
			port_putc(port, ',');
		port_printf(port, "\n\t");
		print_sjis(port, e->values[i].symbol);
		port_printf(port, " = %d", e->values[i].value);
	}
	port_printf(port, "\n};\n");
}
