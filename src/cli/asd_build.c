/* Copyright (C) 2023 kichikuou <KichikuouChrome@gmail.com>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "system4.h"
#include "system4/file.h"
#include "system4/savefile.h"
#include "system4/string.h"
#include "alice.h"
#include "cli.h"

enum {
	LOPT_ENCODE = 256,
	LOPT_OUTPUT,
};

static int json_get_int(cJSON *o, const char *key)
{
	cJSON *v = cJSON_GetObjectItem(o, key);
	if (!cJSON_IsNumber(v))
		ALICE_ERROR("Expected a number for '%s'", key);
	return v->valueint;
}

static char *json_get_string(cJSON *o, const char *key)
{
	cJSON *v = cJSON_GetObjectItem(o, key);
	if (!cJSON_IsString(v))
		ALICE_ERROR("Expected a string for '%s'", key);
	return conv_output(v->valuestring);
}

static cJSON *json_get_array(cJSON *o, const char *key)
{
	cJSON *v = cJSON_GetObjectItem(o, key);
	if (!cJSON_IsArray(v))
		ALICE_ERROR("Expected an array for '%s'", key);
	return v;
}

struct typed_value {
	enum ain_data_type type;
	int32_t value;
};
#define TYPED_VALUE(t, v) ((struct typed_value) { .type = (t), .value = (v) })

static struct typed_value add_value_to_gsave(cJSON *v, struct gsave *save);

static void collect_flat_arrays(cJSON *a, struct gsave_array *array, struct gsave *save)
{
	if (cJSON_IsArray(cJSON_GetArrayItem(a, 0))) {
		cJSON *e;
		cJSON_ArrayForEach(e, a) {
			collect_flat_arrays(e, array, save);
		}
		return;
	}
	array->flat_arrays = xrealloc_array(array->flat_arrays, array->nr_flat_arrays, array->nr_flat_arrays+1, sizeof(struct gsave_flat_array));
	struct gsave_flat_array *fa = &array->flat_arrays[array->nr_flat_arrays++];
	fa->nr_values = cJSON_GetArraySize(a);
	fa->values = xcalloc(fa->nr_values, sizeof(struct gsave_array_value));
	struct gsave_array_value *dest = fa->values;
	cJSON *e;
	cJSON_ArrayForEach(e, a) {
		struct typed_value tv = add_value_to_gsave(e, save);
		dest->type = tv.type;
		dest->value = tv.value;
		dest++;
	}
}

static struct typed_value add_value_to_gsave(cJSON *v, struct gsave *save)
{
	if (cJSON_IsBool(v))
		return TYPED_VALUE(AIN_BOOL, v->valueint);
	if (cJSON_IsNumber(v))
		return TYPED_VALUE(AIN_INT, v->valueint);
	if (cJSON_IsString(v)) {
		struct string *s = string_conv_output(v->valuestring, strlen(v->valuestring));
		int32_t v = gsave_add_string(save, s);
		free_string(s);
		return TYPED_VALUE(AIN_STRING, v);
	}
	if (!cJSON_IsObject(v))
		ERROR("unexpected json object: %s", cJSON_PrintUnformatted(v));

	cJSON *struct_name = cJSON_GetObjectItem(v, "@type");
	if (cJSON_IsString(struct_name)) {
		int nr_fields = cJSON_GetArraySize(v) - 1;
		struct gsave_record rec = {
			.type = GSAVE_RECORD_STRUCT,
			.struct_name = conv_output(struct_name->valuestring),
			.nr_indices = nr_fields,
			.indices = xcalloc(nr_fields, sizeof(int32_t)),
		};
		int32_t *index = rec.indices;
		cJSON *field;
		cJSON_ArrayForEach(field, v) {
			if (!strcmp(field->string, "@type"))
				continue;
			struct typed_value tv = add_value_to_gsave(field, save);
			struct gsave_keyval kv = {
				.name = conv_output(field->string),
				.type = tv.type,
				.value = tv.value
			};
			*index++ = gsave_add_keyval(save, &kv);
		}
		return TYPED_VALUE(AIN_STRUCT, gsave_add_record(save, &rec));
	}

	cJSON *array_rank = cJSON_GetObjectItem(v, "array_rank");
	if (cJSON_IsNumber(array_rank)) {
		struct gsave_array array = { .rank = array_rank->valueint };
		if (array.rank >= 0) {
			cJSON *dims = json_get_array(v, "dimensions");
			array.dimensions = xcalloc(array.rank, sizeof(int32_t));
			for (int i = 0; i < array.rank; i++)
				array.dimensions[i] = cJSON_GetArrayItem(dims, i)->valueint;
			collect_flat_arrays(json_get_array(v, "values"), &array, save);
		}
		return TYPED_VALUE(json_get_int(v, "type"), gsave_add_array(save, &array));
	}

	cJSON *num = cJSON_GetObjectItem(v, "lint");
	if (cJSON_IsNumber(num)) {
		return TYPED_VALUE(AIN_LONG_INT, num->valueint);
	}
	num = cJSON_GetObjectItem(v, "float");
	if (cJSON_IsNumber(num)) {
		union { int32_t i; float f; } u = { .f = num->valuedouble };
		return TYPED_VALUE(AIN_FLOAT, u.i);
	}
	num = cJSON_GetObjectItem(v, "void");
	if (cJSON_IsNumber(num)) {
		return TYPED_VALUE(AIN_VOID, num->valueint);
	}
	num = cJSON_GetObjectItem(v, "functype");
	if (cJSON_IsNumber(num)) {
		return TYPED_VALUE(AIN_FUNC_TYPE, num->valueint);
	}
	num = cJSON_GetObjectItem(v, "ref");
	if (cJSON_IsNumber(num)) {
		return TYPED_VALUE(num->valueint, -1);
	}
	ERROR("unexpected json object: %s", cJSON_PrintUnformatted(v));
}

static struct gsave *json_to_gsave(cJSON *root, bool *encrypt, int *compression_level)
{
	cJSON *save_type = cJSON_GetObjectItem(root, "save_type");
	if (!cJSON_IsString(save_type) || strcmp(save_type->valuestring, "global_save"))
		ALICE_ERROR("not a global_save json");
	cJSON *v = cJSON_GetObjectItem(root, "encrypted");
	if (cJSON_IsBool(v))
		*encrypt = v->valueint;
	v = cJSON_GetObjectItem(root, "compression_level");
	if (cJSON_IsNumber(v))
		*compression_level = v->valueint;

	struct gsave *save = xcalloc(1, sizeof(struct gsave));
	save->key = json_get_string(root, "key");
	save->uk1 = json_get_int(root, "uk1");
	save->version = json_get_int(root, "version");
	save->uk2 = json_get_int(root, "uk2");
	save->nr_ain_globals = json_get_int(root, "num_ain_globals");
	if (save->version >= 5)
		save->group = json_get_string(root, "group");

	cJSON *globals = json_get_array(root, "globals");
	int nr_globals = cJSON_GetArraySize(globals);
	gsave_add_globals_record(save, nr_globals);
	int i;
	cJSON *o;
	cJSON_ArrayForEachIndex(i, o, globals) {
		save->globals[i].name = json_get_string(o, "name");
		cJSON *v = cJSON_GetObjectItem(o, "value");
		struct typed_value tv = add_value_to_gsave(v, save);
		save->globals[i].type = tv.type;
		save->globals[i].value = tv.value;
		save->globals[i].unknown = json_get_int(o, "unknown");
	}
	return save;
}

int command_asd_build(int argc, char *argv[])
{
	bool encode = false;
	const char *output_file = NULL;
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_asd_build);
		if (c == -1)
			break;

		switch (c) {
		case 'e':
		case LOPT_ENCODE:
			encode = true;
			break;
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		USAGE_ERROR(&cmd_asd_build, "Wrong number of arguments.");
	}

	size_t len;
	void *buf = file_read(argv[0], &len);
	if (!buf)
		ALICE_ERROR("Cannot read '%s': %s", argv[0], strerror(errno));

	FILE *out = alice_open_output_file(output_file);

	if (encode) {
		struct savefile save = {
			.buf = buf,
			.len = len,
			.encrypted = true,
			.compression_level = 9
		};
		enum savefile_error error = savefile_write(&save, out);
		if (error != SAVEFILE_SUCCESS)
			ALICE_ERROR("Error writing output: %s", savefile_strerror(error));
		free(buf);
		return 0;
	}

	cJSON *json = cJSON_Parse(buf);
	if (!json)
		ALICE_ERROR("Failed to parse JSON");
	bool encrypt = true;
	int compression_level = 9;
	struct gsave *save = json_to_gsave(json, &encrypt, &compression_level);
	enum savefile_error error = gsave_write(save, out, encrypt, compression_level);
	if (error != SAVEFILE_SUCCESS)
		ALICE_ERROR("Error writing output: %s", savefile_strerror(error));

	gsave_free(save);
	cJSON_Delete(json);
	free(buf);
	return 0;
}

struct command cmd_asd_build = {
	.name = "build",
	.usage = "[options...] <input-file>",
	.description = "Build a save file",
	.parent = &cmd_asd,
	.fun = command_asd_build,
	.options = {
		{ "encode", 'e', "Only compress and encrypt the input file", no_argument, LOPT_ENCODE },
		{ "output", 'o', "Specify the output file path",             required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
