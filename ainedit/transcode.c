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
#include <string.h>
#include "system4.h"
#include "system4/string.h"
#include "ainedit.h"

static char *transcode_cstr(char *str)
{
	if (!str)
		return NULL;
	char *r = encode_text(str);
	free(str);
	return r;
}

static struct string *transcode_string(struct string *str)
{
	char *tmp = encode_text(str->text);
	struct string *r = make_string(tmp, strlen(tmp));
	free(tmp);
	free_string(str);
	return r;
}

static void transcode_variable(struct ain_variable *v)
{
	v->name = transcode_cstr(v->name);
	v->name2 = transcode_cstr(v->name2);
	if (v->has_initval && v->type.data == AIN_STRING)
		v->initval.s = transcode_cstr(v->initval.s);
}

void ain_transcode(struct ain *ain)
{
	for (int i = 0; i < ain->nr_functions; i++) {
		struct ain_function *f = &ain->functions[i];
		f->name = transcode_cstr(f->name);
		for (int i = 0; i < f->nr_vars; i++) {
			transcode_variable(&f->vars[i]);
		}
	}

	for (int i = 0; i < ain->nr_globals; i++) {
		transcode_variable(&ain->globals[i]);
	}

	for (int i = 0; i < ain->nr_initvals; i++) {
		struct ain_initval *v = &ain->global_initvals[i];
		if (v->data_type == AIN_STRING) {
			v->string_value = ain->globals[v->global_index].initval.s;
		}
	}

	for (int i = 0; i < ain->nr_structures; i++) {
		struct ain_struct *s = &ain->structures[i];
		s->name = transcode_cstr(s->name);
		for (int i = 0; i < s->nr_members; i++) {
			transcode_variable(&s->members[i]);
		}
	}

	for (int i = 0; i < ain->nr_messages; i++) {
		ain->messages[i] = transcode_string(ain->messages[i]);
	}

	for (int i = 0; i < ain->nr_strings; i++) {
		ain->strings[i] = transcode_string(ain->strings[i]);
	}

	for (int i = 0; i < ain->nr_filenames; i++) {
		ain->filenames[i] = transcode_cstr(ain->filenames[i]);
	}

	for (int i = 0; i < ain->nr_function_types; i++) {
		struct ain_function_type *f = &ain->function_types[i];
		f->name = transcode_cstr(f->name);
		for (int i = 0; i < f->nr_variables; i++) {
			transcode_variable(&f->variables[i]);
		}
	}

	for (int i = 0; i < ain->nr_delegates; i++) {
		struct ain_function_type *f = &ain->delegates[i];
		f->name = transcode_cstr(f->name);
		for (int i = 0; i < f->nr_variables; i++) {
			transcode_variable(&f->variables[i]);
		}
	}

	for (int i = 0; i < ain->nr_global_groups; i++) {
		ain->global_group_names[i] = transcode_cstr(ain->global_group_names[i]);
	}

	for (int i = 0; i < ain->nr_enums; i++) {
		struct ain_enum *e = &ain->enums[i];
		e->name = transcode_cstr(e->name);
		// NOTE: symbols don't matter
	}
}
