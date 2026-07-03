/* Copyright (C) 2026 kichikuou <KichikuouChrome@gmail.com>
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

#ifndef ALICE_JSON_H
#define ALICE_JSON_H

#include <stdbool.h>
#include "cJSON.h"

// File I/O
cJSON *json_parse_file(const char *path);
void json_write_file(const char *path, const cJSON *json);

// Create a JSON string from a value in the internal encoding, converting it to
// the output encoding.
cJSON *json_create_string(const char *s);

// Required-field accessors: raise an error if the field is missing or has the
// wrong type.
int json_get_int(const cJSON *o, const char *name);
double json_get_double(const cJSON *o, const char *name);
bool json_get_bool(const cJSON *o, const char *name);
const char *json_get_string(const cJSON *o, const char *name);
cJSON *json_get_array(const cJSON *o, const char *name);

// Optional-field accessors: return the default (or NULL) if the field is
// missing, but raise an error if it is present with the wrong type.
int json_get_int_or(const cJSON *o, const char *name, int def);
double json_get_double_or(const cJSON *o, const char *name, double def);
bool json_get_bool_or(const cJSON *o, const char *name, bool def);
const char *json_get_string_or_null(const cJSON *o, const char *name);
cJSON *json_get_array_or_null(const cJSON *o, const char *name);

// String accessors returning a newly-allocated copy.
char *json_dup_string(const cJSON *o, const char *name);
char *json_dup_string_or_null(const cJSON *o, const char *name);

#endif /* ALICE_JSON_H */
