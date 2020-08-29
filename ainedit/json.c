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
#include "cJSON.h"
#include "system4.h"
#include "system4/ain.h"

static bool cJSON_GetObjectBool(const cJSON * const o, const char * const name, bool def)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (v && !cJSON_IsBool(v))
		ERROR("Expected a boolean for '%s'", name);
	return v ? v->valueint : def;
}

static int cJSON_GetObjectInteger(const cJSON * const o, const char * const name, int def)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (v && !cJSON_IsNumber(v))
		ERROR("Expected a number for '%s'", name);
	return v ? v->valueint : def;
}

static int cJSON_GetObjectInteger_NonNull(const cJSON * const o, const char * const name)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (!v || !cJSON_IsNumber(v))
		ERROR("Expected a number for '%s'", name);
	return v->valueint;
}

static char *cJSON_GetObjectString(const cJSON * const o, const char * const name)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (v && !cJSON_IsString(v))
		ERROR("Expected string for '%s'", name);
	return v ? strdup(v->valuestring) : NULL;
}

static char *cJSON_GetObjectString_NonNull(const cJSON * const o, const char * const name)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (!v || !cJSON_IsString(v))
		ERROR("Expected string for '%s'", name);
	return strdup(v->valuestring);
}

static cJSON *cJSON_GetObjectArray(const cJSON * const o, const char * const name)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (v && !cJSON_IsArray(v))
		ERROR("Expected an array for '%s'", name);
	return v;
}

static cJSON *cJSON_GetObjectArray_NonNull(const cJSON * const o, const char * const name)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (!v || !cJSON_IsArray(v))
		ERROR("Expected an array for '%s'", name);
	return v;
}

static void _read_type_declaration(cJSON *decl, struct ain_type *dst)
{
	dst->data  = cJSON_GetArrayItem(decl, 0)->valueint;
	dst->struc = cJSON_GetArrayItem(decl, 1)->valueint;
	dst->rank  = cJSON_GetArrayItem(decl, 2)->valueint;
}

static void read_type_declaration(cJSON *decl, struct ain_type *dst)
{
	int size = cJSON_GetArraySize(decl);
	if (size < 3 || size > 4)
		ERROR("Invalid type declaration (array size = %d)", size);

	_read_type_declaration(decl, dst);
	if (size == 4) {
		int i;
		cJSON *v, *a = cJSON_GetArrayItem(decl, 4);
		if (!cJSON_IsArray(a))
			ERROR("Non-array in array-type slot");
		if (cJSON_GetArraySize(a) == 0)
			return;

		dst->array_type = xcalloc(cJSON_GetArraySize(a), sizeof(struct ain_variable));
		cJSON_ArrayForEachIndex(i, v, a) {
			if (!cJSON_IsArray(v))
				ERROR("Non-array in array-type list");
			if (cJSON_GetArraySize(v) != 3)
				ERROR("Invalid type declaration (array size = %d)", cJSON_GetArraySize(v));
			_read_type_declaration(v, &dst->array_type[i]);
			dst->array_type[i].array_type = &dst->array_type[i+1];
		}
		dst->array_type[i-1].array_type = NULL;
	}
}

static void read_variable_declaration(cJSON *decl, struct ain_variable *dst)
{
	dst->name = cJSON_GetObjectString_NonNull(decl, "name");
	dst->name2 = cJSON_GetObjectString(decl, "name2");
	read_type_declaration(cJSON_GetObjectArray_NonNull(decl, "type"), &dst->type);

	cJSON *v = cJSON_GetObjectItem(decl, "initval");
	if (v) {
		switch (dst->type.data) {
		case AIN_STRING:
			if (!cJSON_IsString(v))
				ERROR("Non-string initval for string variable");
			dst->initval.s = strdup(v->valuestring);
			break;
		case AIN_FLOAT:
			if (!cJSON_IsNumber(v))
				ERROR("Non-number initval for float variable");
			dst->initval.f = v->valuedouble;
			break;
		default:
			if (!cJSON_IsNumber(v))
				ERROR("Non-number initval for variable");
			dst->initval.i = v->valueint;
			break;
		}
	}

	dst->group_index = cJSON_GetObjectInteger(decl, "group-index", -1);
}

static void read_function_declaration(cJSON *decl, struct ain_function *dst)
{
	int i;
	cJSON *args, *vars, *v;

	dst->address = cJSON_GetObjectInteger(decl, "address", 0);
	dst->name = cJSON_GetObjectString_NonNull(decl, "name");
	dst->is_label = cJSON_GetObjectBool(decl, "is-label", 0);
	read_type_declaration(cJSON_GetObjectArray_NonNull(decl, "return-type"), &dst->return_type);
	dst->is_lambda = cJSON_GetObjectInteger(decl, "unknown-bool", 0);
	dst->crc = cJSON_GetObjectInteger(decl, "crc", 0);

	args = cJSON_GetObjectArray_NonNull(decl, "arguments");
	vars = cJSON_GetObjectArray_NonNull(decl, "variables");
	dst->nr_args = cJSON_GetArraySize(args);
	dst->nr_vars = dst->nr_args + cJSON_GetArraySize(vars);

	i = 0;
	dst->vars = xcalloc(dst->nr_vars, sizeof(struct ain_variable));
	cJSON_ArrayForEach(v, args) {
		if (!cJSON_IsObject(v))
			ERROR("Non-object in argument list");
		read_variable_declaration(v, &dst->vars[i]);
		i++;
	}
	cJSON_ArrayForEach(v, vars) {
		if (!cJSON_IsObject(v))
			ERROR("Non-object in variable list");
		read_variable_declaration(v, &dst->vars[i]);
		i++;
	}
}

static void read_function_declarations(cJSON *decl, struct ain *ain)
{
	int i;
	cJSON *f;
	struct ain_function *functions = xcalloc(cJSON_GetArraySize(decl), sizeof(struct ain_function));
	cJSON_ArrayForEachIndex(i, f, decl) {
		if (!cJSON_IsObject(f))
			ERROR("Non-object in function list");
		read_function_declaration(f, &functions[i]);
	}

	ain_free_functions(ain);
	ain->functions = functions;
	ain->nr_functions = i;
}

static struct ain_variable *read_variable_declarations(cJSON *decl, int *n)
{
	cJSON *v;
	int i;
	struct ain_variable *vars = xcalloc(cJSON_GetArraySize(decl), sizeof(struct ain_variable));
	cJSON_ArrayForEachIndex(i, v, decl) {
		if (!cJSON_IsObject(v))
			ERROR("Non-object in variable list");
		read_variable_declaration(v, &vars[i]);
	}
	*n = i;
	return vars;
}

static void read_global_declarations(cJSON *decl, struct ain *ain)
{
	ain_free_globals(ain);
	ain->globals = read_variable_declarations(decl, &ain->nr_globals);
}

static struct ain_interface *read_interface_list(cJSON *decl, int32_t *n)
{
	*n = cJSON_GetArraySize(decl);
	struct ain_interface *iface = xcalloc(*n, sizeof(struct ain_interface));

	cJSON *v;
	int i;
	cJSON_ArrayForEachIndex(i, v, decl) {
		if (!cJSON_IsArray(v))
			ERROR("Non-array in interface list");
		if (cJSON_GetArraySize(v) != 2)
			ERROR("Wrong size array in interface list");
		iface[i].struct_type = cJSON_GetArrayItem(v, 0)->valueint;
		iface[i].uk = cJSON_GetArrayItem(v, 1)->valueint;
	}

	*n = i;
	return iface;
}

static void read_structure_declaration(cJSON *decl, struct ain_struct *dst)
{
	cJSON *a;
	dst->name = cJSON_GetObjectString_NonNull(decl, "name");
	if ((a = cJSON_GetObjectArray(decl, "interfaces"))) {
		dst->interfaces = read_interface_list(a, &dst->nr_interfaces);
	}
	dst->constructor = cJSON_GetObjectInteger(decl, "constructor", -1);
	dst->destructor = cJSON_GetObjectInteger(decl, "destructor", -1);
	if ((a = cJSON_GetObjectArray(decl, "members"))) {
		dst->members = read_variable_declarations(a, &dst->nr_members);
	}
}

static void read_structure_declarations(cJSON *decl, struct ain *ain)
{
	int i;
	cJSON *s;
	struct ain_struct *structs = xcalloc(cJSON_GetArraySize(decl), sizeof(struct ain_struct));
	cJSON_ArrayForEachIndex(i, s, decl) {
		if (!cJSON_IsObject(s))
			ERROR("Non-object in structure list");
		read_structure_declaration(s, &structs[i]);
	}

	ain_free_structures(ain);
	ain->structures = structs;
	ain->nr_structures = i;
}

static void read_library_declaration(cJSON *decl, struct ain_library *dst)
{
	dst->name = cJSON_GetObjectString_NonNull(decl, "name");

	int i;
	cJSON *f, *jfuns = cJSON_GetObjectArray_NonNull(decl, "functions");
	struct ain_hll_function *funs = xcalloc(cJSON_GetArraySize(jfuns), sizeof(struct ain_hll_function));
	cJSON_ArrayForEachIndex(i, f, jfuns) {
		funs[i].name = cJSON_GetObjectString_NonNull(f, "name");
		funs[i].return_type.data = cJSON_GetObjectInteger_NonNull(f, "return-type");
		// TODO: v14 has full variable type
		funs[i].return_type.struc = -1;
		funs[i].return_type.rank = 0;

		int j;
		cJSON *arg, *jargs = cJSON_GetObjectArray_NonNull(f, "arguments");
		struct ain_hll_argument *args = xcalloc(cJSON_GetArraySize(jargs), sizeof(struct ain_hll_argument));
		cJSON_ArrayForEachIndex(j, arg, jargs) {
			args[j].name = cJSON_GetObjectString_NonNull(arg, "name");
			args[j].type.data = cJSON_GetObjectInteger_NonNull(arg, "type");
			// TODO: v14 has full variable type
			args[j].type.struc = -1;
			args[j].type.rank = 0;
		}
		funs[i].nr_arguments = j;
		funs[i].arguments = args;
	}

	dst->nr_functions = i;
	dst->functions = funs;
}

static void read_library_declarations(cJSON *decl, struct ain *ain)
{
	int i;
	cJSON *lib;
	struct ain_library *libs = xcalloc(cJSON_GetArraySize(decl), sizeof(struct ain_library));
	cJSON_ArrayForEachIndex(i, lib, decl) {
		if (!cJSON_IsObject(lib))
			ERROR("Non-object in library list");
		read_library_declaration(lib, &libs[i]);
	}

	ain_free_libraries(ain);
	ain->libraries = libs;
	ain->nr_libraries = i;
}

static void read_switch_declaration(cJSON *decl, struct ain_switch *dst)
{
	dst->case_type = cJSON_GetObjectInteger_NonNull(decl, "case-type");
	dst->default_address = cJSON_GetObjectInteger_NonNull(decl, "default-address");

	int i;
	cJSON *v, *a = cJSON_GetObjectArray_NonNull(decl, "cases");
	struct ain_switch_case *cases = xcalloc(cJSON_GetArraySize(a), sizeof(struct ain_switch_case));
	cJSON_ArrayForEachIndex(i, v, a) {
		if (!cJSON_IsObject(v))
			ERROR("Non-object in switch case list");
		cases[i].value = cJSON_GetObjectInteger_NonNull(v, "value");
		cases[i].address = cJSON_GetObjectInteger_NonNull(v, "address");
	}
	dst->cases = cases;
	dst->nr_cases = i;
}

static void read_switch_declarations(cJSON *decl, struct ain *ain)
{
	int i;
	cJSON *s;
	struct ain_switch *switches = xcalloc(cJSON_GetArraySize(decl), sizeof(struct ain_switch));
	cJSON_ArrayForEachIndex(i, s, decl) {
		if (!cJSON_IsObject(s))
			ERROR("Non-object in switch list");
		read_switch_declaration(s, &switches[i]);
	}

	ain_free_switches(ain);
	ain->switches = switches;
	ain->nr_switches = i;
}

static char **read_string_array(cJSON *decl, int *n)
{
	int i;
	cJSON *str;
	char **strings = xcalloc(cJSON_GetArraySize(decl), sizeof(char*));
	cJSON_ArrayForEachIndex(i, str, decl) {
		if (!cJSON_IsString(str))
			ERROR("Non-string in string list");
		strings[i] = strdup(str->valuestring);
	}
	*n = i;
	return strings;
}

static void read_filename_declarations(cJSON *decl, struct ain *ain)
{
	ain_free_filenames(ain);
	ain->filenames = read_string_array(decl, &ain->nr_filenames);
}

static void read_function_type_declaration(cJSON *decl, struct ain_function_type *dst)
{
	dst->name = cJSON_GetObjectString_NonNull(decl, "name");
	read_type_declaration(cJSON_GetObjectArray_NonNull(decl, "return-type"), &dst->return_type);

	int i = 0;
	cJSON *v;
	cJSON *args = cJSON_GetObjectArray_NonNull(decl, "arguments");
	cJSON *vars = cJSON_GetObjectArray_NonNull(decl, "variables");
	dst->nr_arguments = cJSON_GetArraySize(args);
	dst->nr_variables = cJSON_GetArraySize(vars) + dst->nr_arguments;
	struct ain_variable *variables = xcalloc(dst->nr_variables, sizeof(struct ain_variable));
	cJSON_ArrayForEach(v, args) {
		if (!cJSON_IsObject(v))
			ERROR("Non-object in argument list");
		read_variable_declaration(v, &variables[i]);
		i++;
	}
	cJSON_ArrayForEach(v, vars) {
		if (!cJSON_IsObject(v))
			ERROR("Non-object in variable list");
		read_variable_declaration(v, &variables[i]);
		i++;
	}
	dst->variables = variables;
}

static struct ain_function_type *_read_function_type_declarations(cJSON *decl, int *n)
{
	int i;
	cJSON *f;
	struct ain_function_type *types = xcalloc(cJSON_GetArraySize(decl), sizeof(struct ain_function_type));
	cJSON_ArrayForEachIndex(i, f, decl) {
		if (!cJSON_IsObject(f))
			ERROR("Non-object in function type list");
		read_function_type_declaration(f, &types[i]);
	}
	*n = i;
	return types;
}

static void read_function_type_declarations(cJSON *decl, struct ain *ain)
{
	ain_free_function_types(ain);
	ain->function_types = _read_function_type_declarations(decl, &ain->nr_function_types);
}

static void read_delegate_declarations(cJSON *decl, struct ain *ain)
{
	ain_free_delegates(ain);
	ain->delegates = _read_function_type_declarations(decl, &ain->nr_delegates);
}

static void read_global_group_declarations(cJSON *decl, struct ain *ain)
{
	ain_free_global_groups(ain);
	ain->global_group_names = read_string_array(decl, &ain->nr_global_groups);
}

static void read_enum_declarations(cJSON *decl, struct ain *ain)
{
	int i;
	cJSON *e;
	struct ain_enum *enums = xcalloc(cJSON_GetArraySize(decl), sizeof(struct ain_enum));
	cJSON_ArrayForEachIndex(i, e, decl) {
		if (!cJSON_IsObject(e))
			ERROR("Non-object in enum list");
		enums[i].name = cJSON_GetObjectString_NonNull(e, "name");

		int j;
		cJSON *s, *syms = cJSON_GetObjectArray_NonNull(e, "values");
		char **symbols = xcalloc(cJSON_GetArraySize(syms), sizeof(char*));
		cJSON_ArrayForEachIndex(j, s, syms) {
			if (!cJSON_IsString(s))
				ERROR("Non-string in enum symbol list");
			symbols[i] = strdup(s->valuestring);
		}
		enums[i].nr_symbols = j;
		enums[i].symbols = symbols;
	}

	ain_free_enums(ain);
	ain->enums = enums;
	ain->nr_enums = i;
}

static void read_json_declarations(cJSON *decl, struct ain *ain)
{
	cJSON *v;

	// VERS
	ain->version = cJSON_GetObjectInteger(decl, "version", 0);
	// KEYC
	ain->keycode = cJSON_GetObjectInteger(decl, "keycode", 0);
	// FUNC
	if ((v = cJSON_GetObjectArray(decl, "functions")))
		read_function_declarations(v, ain);
	// GLOB
	if ((v = cJSON_GetObjectArray(decl, "globals")))
		read_global_declarations(v, ain);
	// STRT
	if ((v = cJSON_GetObjectArray(decl, "structures")))
		read_structure_declarations(v, ain);
	// MAIN
	ain->main = cJSON_GetObjectInteger(decl, "main", 0);
	// MSGF
	ain->msgf = cJSON_GetObjectInteger(decl, "msgf", 0);
	// HLL0
	if ((v = cJSON_GetObjectArray(decl, "libraries")))
		read_library_declarations(v, ain);
	// SWI0
	if ((v = cJSON_GetObjectArray(decl, "switches")))
		read_switch_declarations(v, ain);
	// GVER
	ain->game_version = cJSON_GetObjectInteger(decl, "game-version", 0);
	// FNAM
	if ((v = cJSON_GetObjectArray(decl, "filenames")))
		read_filename_declarations(v, ain);
	// OJMP
	ain->ojmp = cJSON_GetObjectInteger(decl, "ojmp", 0);
	// FNCT
	if ((v = cJSON_GetObjectArray(decl, "function-types")))
		read_function_type_declarations(v, ain);
	// DELG
	if ((v = cJSON_GetObjectArray(decl, "delegates")))
		read_delegate_declarations(v, ain);
	// OBJG
	if ((v = cJSON_GetObjectArray(decl, "global-groups")))
		read_global_group_declarations(v, ain);
	// ENUM
	if ((v = cJSON_GetObjectArray(decl, "enums")))
		read_enum_declarations(v, ain);
}

void read_declarations(const char *filename, struct ain *ain)
{
	FILE *f;
	long len;
	char *buf;

	if (!(f = fopen(filename, "r")))
		ERROR("Failed to open '%s': %s", filename, strerror(errno));

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	buf = xmalloc(len + 1);
	if (fread(buf, len, 1, f) != 1)
		ERROR("Failed to read '%s': %s", filename, strerror(errno));

	if (fclose(f))
		ERROR("Failed to close '%s': %s", filename, strerror(errno));

	cJSON *j = cJSON_Parse(buf);
	if (!j)
		ERROR("Failed to parse JSON file '%s'", filename);

	read_json_declarations(j, ain);
}
