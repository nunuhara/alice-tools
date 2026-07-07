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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "cJSON.h"
#include "system4.h"
#include "system4/file.h"
#include "alice.h"
#include "alice/json.h"

cJSON *json_parse_file(const char *path)
{
	size_t size;
	uint8_t *data = file_read(path, &size);
	if (!data)
		ALICE_ERROR("reading '%s': %s", path, strerror(errno));
	uint8_t *buf = xmalloc(size + 1);
	memcpy(buf, data, size);
	buf[size] = '\0';
	free(data);
	cJSON *j = cJSON_Parse((char*)buf);
	if (!j)
		ALICE_ERROR("Failed to parse JSON: %s", path);
	free(buf);
	return j;
}

void json_write_file(const char *path, const cJSON *json)
{
	char *text = cJSON_Print(json);
	if (!text)
		ALICE_ERROR("cJSON_Print failed");
	FILE *f = checked_fopen(path, "wb");
	if (fputs(text, f) == EOF)
		ALICE_ERROR("write '%s': %s", path, strerror(errno));
	fclose(f);
	free(text);
}

cJSON *json_create_string(const char *s)
{
	char *u = conv_output(s);
	cJSON *json = cJSON_CreateString(u);
	free(u);
	return json;
}

static cJSON *get_required(const cJSON *o, const char *name)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (!v)
		ALICE_ERROR("JSON: missing required field '%s'", name);
	return v;
}

int json_get_int(const cJSON *o, const char *name)
{
	cJSON *v = get_required(o, name);
	if (!cJSON_IsNumber(v))
		ALICE_ERROR("JSON: expected number for '%s'", name);
	return v->valueint;
}

int json_get_int_or(const cJSON *o, const char *name, int def)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (!v)
		return def;
	if (!cJSON_IsNumber(v))
		ALICE_ERROR("JSON: expected number for '%s'", name);
	return v->valueint;
}

double json_get_double(const cJSON *o, const char *name)
{
	cJSON *v = get_required(o, name);
	if (!cJSON_IsNumber(v))
		ALICE_ERROR("JSON: expected number for '%s'", name);
	return v->valuedouble;
}

double json_get_double_or(const cJSON *o, const char *name, double def)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (!v)
		return def;
	if (!cJSON_IsNumber(v))
		ALICE_ERROR("JSON: expected number for '%s'", name);
	return v->valuedouble;
}

bool json_get_bool(const cJSON *o, const char *name)
{
	cJSON *v = get_required(o, name);
	if (!cJSON_IsBool(v))
		ALICE_ERROR("JSON: expected boolean for '%s'", name);
	return cJSON_IsTrue(v);
}

bool json_get_bool_or(const cJSON *o, const char *name, bool def)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (!v)
		return def;
	if (!cJSON_IsBool(v))
		ALICE_ERROR("JSON: expected boolean for '%s'", name);
	return cJSON_IsTrue(v);
}

const char *json_get_string(const cJSON *o, const char *name)
{
	cJSON *v = get_required(o, name);
	if (!cJSON_IsString(v))
		ALICE_ERROR("JSON: expected string for '%s'", name);
	return v->valuestring;
}

const char *json_get_string_or_null(const cJSON *o, const char *name)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (!v || cJSON_IsNull(v))
		return NULL;
	if (!cJSON_IsString(v))
		ALICE_ERROR("JSON: expected string for '%s'", name);
	return v->valuestring;
}

char *json_dup_string(const cJSON *o, const char *name)
{
	const char *s = json_get_string(o, name);
	return strdup(s);
}

char *json_dup_string_or_null(const cJSON *o, const char *name)
{
	const char *s = json_get_string_or_null(o, name);
	return s ? strdup(s) : NULL;
}

cJSON *json_get_array(const cJSON *o, const char *name)
{
	cJSON *v = get_required(o, name);
	if (!cJSON_IsArray(v))
		ALICE_ERROR("JSON: expected array for '%s'", name);
	return v;
}

cJSON *json_get_array_or_null(const cJSON *o, const char *name)
{
	cJSON *v = cJSON_GetObjectItem(o, name);
	if (v && !cJSON_IsArray(v))
		ALICE_ERROR("JSON: expected array for '%s'", name);
	return v;
}
