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
#include "alice.h"
#include "cJSON.h"
#include "system4.h"
#include "system4/ain.h"

static cJSON *_ain_type_to_json(possibly_unused struct ain *ain, struct ain_type *t)
{
	cJSON *a = cJSON_CreateArray();
	cJSON_AddItemToArray(a, cJSON_CreateNumber(t->data));
	cJSON_AddItemToArray(a, cJSON_CreateNumber(t->struc));
	cJSON_AddItemToArray(a, cJSON_CreateNumber(t->rank));
	return a;
}

static cJSON *ain_type_to_json(struct ain *ain, struct ain_type *t)
{
	cJSON *a = _ain_type_to_json(ain, t);
	if (t->array_type) {
		cJSON_AddItemToArray(a, ain_type_to_json(ain, t->array_type));
	} else {
		cJSON_AddItemToArray(a, cJSON_CreateNull());
	}
	return a;
}

static cJSON *ain_variable_to_json(struct ain *ain, struct ain_variable *var)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "name", var->name);
	if (var->name2)
		cJSON_AddStringToObject(o, "name2", var->name2);
	cJSON_AddItemToObject(o, "type", ain_type_to_json(ain, &var->type));
	if (var->has_initval) {
		switch (var->type.data) {
		case AIN_STRING:
			cJSON_AddStringToObject(o, "initval", var->initval.s);
			break;
		case AIN_FLOAT:
			cJSON_AddNumberToObject(o, "initval", var->initval.f);
			break;
		default:
			cJSON_AddNumberToObject(o, "initval", var->initval.i);
		}
	}
	if (var->group_index >= 0)
		cJSON_AddNumberToObject(o, "group-index", var->group_index);
	return o;
}

static cJSON *ain_function_to_json(struct ain *ain, struct ain_function *f)
{
	cJSON *a;
	cJSON *o = cJSON_CreateObject();
	cJSON_AddNumberToObject(o, "index", f - ain->functions);
	cJSON_AddNumberToObject(o, "address", f->address);
	cJSON_AddStringToObject(o, "name", f->name);
	if (f->is_label)
		cJSON_AddBoolToObject(o, "is-label", true);
	cJSON_AddItemToObject(o, "return-type", ain_type_to_json(ain, &f->return_type));
	if (f->is_lambda)
		cJSON_AddBoolToObject(o, "unknown-bool", true);
	cJSON_AddNumberToObject(o, "crc", f->crc);

	a = cJSON_CreateArray();
	for (int i = 0; i < f->nr_args; i++) {
		cJSON_AddItemToArray(a, ain_variable_to_json(ain, &f->vars[i]));
	}
	cJSON_AddItemToObject(o, "arguments", a);

	a = cJSON_CreateArray();
	for (int i = f->nr_args; i < f->nr_vars; i++) {
		cJSON_AddItemToArray(a, ain_variable_to_json(ain, &f->vars[i]));
	}
	cJSON_AddItemToObject(o, "variables", a);
	return o;
}

static cJSON *ain_structure_to_json(struct ain *ain, struct ain_struct *s)
{
	cJSON *a;
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "name", s->name);

	if (s->nr_interfaces > 0) {
		a = cJSON_CreateArray();
		for (int i = 0; i < s->nr_interfaces; i++) {
			cJSON *iface = cJSON_CreateArray();
			cJSON_AddItemToArray(iface, cJSON_CreateNumber(s->interfaces[i].struct_type)); // TODO: use struct name
			cJSON_AddItemToArray(iface, cJSON_CreateNumber(s->interfaces[i].vtable_offset));
			cJSON_AddItemToArray(a, iface);
		}
		cJSON_AddItemToObject(o, "interfaces", a);
	}

	if (s->constructor >= 0)
		cJSON_AddNumberToObject(o, "constructor", s->constructor); // TODO: use function name
	if (s->destructor >= 0)
		cJSON_AddNumberToObject(o, "destructor", s->destructor); // TODO: use function name

	a = cJSON_CreateArray();
	for (int i = 0; i < s->nr_members; i++) {
		cJSON_AddItemToArray(a, ain_variable_to_json(ain, &s->members[i]));
	}
	cJSON_AddItemToObject(o, "members", a);

	return o;
}

static cJSON *ain_library_to_json(struct ain *ain, struct ain_library *lib)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "name", lib->name);

	cJSON *functions = cJSON_CreateArray();
	for (int i = 0; i < lib->nr_functions; i++) {
		cJSON *f = cJSON_CreateObject();
		cJSON_AddStringToObject(f, "name", lib->functions[i].name);
		if (AIN_VERSION_GTE(ain, 14, 0)) {
			cJSON_AddItemToObject(f, "return-type",
					ain_type_to_json(ain, &lib->functions[i].return_type));
		} else {
			cJSON_AddNumberToObject(f, "return-type",
					lib->functions[i].return_type.data);
		}

		cJSON *args = cJSON_CreateArray();
		for (int j = 0; j < lib->functions[i].nr_arguments; j++) {
			struct ain_hll_argument *a = &lib->functions[i].arguments[j];
			cJSON *arg = cJSON_CreateObject();
			cJSON_AddStringToObject(arg, "name", a->name);
			if (AIN_VERSION_GTE(ain, 14, 0)) {
				cJSON_AddItemToObject(arg, "type", ain_type_to_json(ain, &a->type));
			} else {
				cJSON_AddNumberToObject(arg, "type", a->type.data);
			}
			cJSON_AddItemToArray(args, arg);
		}
		cJSON_AddItemToObject(f, "arguments", args);
		cJSON_AddItemToArray(functions, f);
	}
	cJSON_AddItemToObject(o, "functions", functions);

	return o;
}

static cJSON *ain_switch_to_json(struct ain_switch *sw)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddNumberToObject(o, "case-type", sw->case_type);
	cJSON_AddNumberToObject(o, "default-address", sw->default_address);

	cJSON *a = cJSON_CreateArray();
	for (int i = 0; i < sw->nr_cases; i++) {
		cJSON *scase = cJSON_CreateObject();
		cJSON_AddNumberToObject(scase, "value", sw->cases[i].value);
		cJSON_AddNumberToObject(scase, "address", sw->cases[i].address);
		cJSON_AddItemToArray(a, scase);
	}
	cJSON_AddItemToObject(o, "cases", a);

	return o;
}

static cJSON *ain_scenario_label_to_json(struct ain_scenario_label *label)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "name", label->name);
	cJSON_AddNumberToObject(o, "address", label->address);
	return o;
}

static cJSON *ain_function_type_to_json(struct ain *ain, struct ain_function_type *ft)
{
	cJSON *a;
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "name", ft->name);
	cJSON_AddItemToObject(o, "return-type", ain_type_to_json(ain, &ft->return_type));

	a = cJSON_CreateArray();
	for (int i = 0; i < ft->nr_arguments; i++) {
		cJSON_AddItemToArray(a, ain_variable_to_json(ain, &ft->variables[i]));
	}
	cJSON_AddItemToObject(o, "arguments", a);

	a = cJSON_CreateArray();
	for (int i = ft->nr_arguments; i < ft->nr_variables; i++) {
		cJSON_AddItemToArray(a, ain_variable_to_json(ain, &ft->variables[i]));
	}
	cJSON_AddItemToObject(o, "variables", a);

	return o;
}

static cJSON *ain_enum_to_json(struct ain_enum *e)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "name", e->name);

	cJSON *a = cJSON_CreateArray();
	for (int i = 0; i < e->nr_values; i++) {
		cJSON *v = cJSON_CreateArray();
		cJSON_AddItemToArray(v, cJSON_CreateString(e->values[i].symbol));
		cJSON_AddItemToArray(v, cJSON_CreateNumber(e->values[i].value));
		cJSON_AddItemToArray(a, v);
	}
	cJSON_AddItemToObject(o, "values", a);

	return o;
}

static cJSON *ain_to_json(struct ain *ain)
{
	cJSON *a;
	cJSON *j = cJSON_CreateObject();

	// VERS: ain version
	cJSON_AddNumberToObject(j, "version", ain->version);

	// KEYC: keycode
	cJSON_AddNumberToObject(j, "keycode", ain->keycode);

	// FUNC: functions
	a = cJSON_CreateArray();
	for (int i = 0; i < ain->nr_functions; i++) {
		cJSON_AddItemToArray(a, ain_function_to_json(ain, &ain->functions[i]));
	}
	cJSON_AddItemToObject(j, "functions", a);

	// GLOB: globals
	a = cJSON_CreateArray();
	for (int i = 0; i < ain->nr_globals; i++) {
		cJSON_AddItemToArray(a, ain_variable_to_json(ain, &ain->globals[i]));
	}
	cJSON_AddItemToObject(j, "globals", a);

	// STRT: structures
	a = cJSON_CreateArray();
	for (int i = 0; i < ain->nr_structures; i++) {
		cJSON_AddItemToArray(a, ain_structure_to_json(ain, &ain->structures[i]));
	}
	cJSON_AddItemToObject(j, "structures", a);

	// MAIN: main function index
	cJSON_AddNumberToObject(j, "main", ain->main);

	// MSGF: message function index
	cJSON_AddNumberToObject(j, "msgf", ain->msgf);

	// HLL0: libraries
	a = cJSON_CreateArray();
	for (int i = 0; i < ain->nr_libraries; i++) {
		cJSON_AddItemToArray(a, ain_library_to_json(ain, &ain->libraries[i]));
	}
	cJSON_AddItemToObject(j, "libraries", a);

	// SWI0: switch
	a = cJSON_CreateArray();
	for (int i = 0; i < ain->nr_switches; i++) {
		cJSON_AddItemToArray(a, ain_switch_to_json(&ain->switches[i]));
	}
	cJSON_AddItemToObject(j, "switches", a);

	// GVER: game version
	cJSON_AddNumberToObject(j, "game-version", ain->game_version);

	// SLBL: scenario labels
	if (ain->nr_scenario_labels > 0) {
		a = cJSON_CreateArray();
		for (int i = 0; i < ain->nr_scenario_labels; i++) {
			cJSON_AddItemToArray(a, ain_scenario_label_to_json(&ain->scenario_labels[i]));
		}
		cJSON_AddItemToObject(j, "scenario-labels", a);
	}

	// FNAM: filenames
	a = cJSON_CreateArray();
	for (int i = 0; i < ain->nr_filenames; i++) {
		cJSON_AddItemToArray(a, cJSON_CreateString(ain->filenames[i]));
	}
	cJSON_AddItemToObject(j, "filenames", a);

	// OJMP: ???
	cJSON_AddNumberToObject(j, "ojmp", ain->ojmp);

	// FNCT: function types
	if (ain->nr_function_types > 0) {
		a = cJSON_CreateArray();
		for (int i = 0; i < ain->nr_function_types; i++) {
			cJSON_AddItemToArray(a, ain_function_type_to_json(ain, &ain->function_types[i]));
		}
		cJSON_AddItemToObject(j, "function-types", a);
	}

	// DELG: delegates
	if (ain->nr_delegates > 0) {
		a = cJSON_CreateArray();
		for (int i = 0; i < ain->nr_delegates; i++) {
			cJSON_AddItemToArray(a, ain_function_type_to_json(ain, &ain->delegates[i]));
		}
		cJSON_AddItemToObject(j, "delegates", a);
	}

	// OBJG: global group names
	if (ain->nr_global_groups > 0) {
		a = cJSON_CreateArray();
		for (int i = 0; i < ain->nr_global_groups; i++) {
			cJSON_AddItemToArray(a, cJSON_CreateString(ain->global_group_names[i]));
		}
		cJSON_AddItemToObject(j, "global-groups", a);
	}

	// ENUM: enumerations
	if (ain->nr_enums > 0) {
		a = cJSON_CreateArray();
		for (int i = 0; i < ain->nr_enums; i++) {
			cJSON_AddItemToArray(a, ain_enum_to_json(&ain->enums[i]));
		}
		cJSON_AddItemToObject(j, "enums", a);
	}

	return j;
}

void ain_dump_json(FILE *out, struct ain *ain)
{
	cJSON *j = ain_to_json(ain);
	char *str = cJSON_Print(j);

	if (fwrite(str, strlen(str), 1, out) != 1)
		ERROR("Failed to write to file: %s", strerror(errno));

	if (fclose(out))
		ERROR("Failed to close file: %s", strerror(errno));

	free(str);
	cJSON_Delete(j);
}
