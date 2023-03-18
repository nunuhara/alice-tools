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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "system4.h"
#include "system4/savefile.h"
#include "system4/string.h"
#include "alice.h"
#include "cli.h"

enum {
	LOPT_DECODE = 256,
	LOPT_OUTPUT,
};

static cJSON *to_json_string(const char *s)
{
	char *u = conv_output(s);
	cJSON *json = cJSON_CreateString(u);
	free(u);
	return json;
}

static cJSON *create_number_wrapper(const char *key, double value)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddNumberToObject(o, key, value);
	return o;
}

static cJSON *value_to_json(int32_t value, enum ain_data_type type, struct gsave *save);

static cJSON *array_to_json(int rank, int32_t *dims, struct gsave_flat_array **fa, struct gsave *save)
{
	cJSON *a = cJSON_CreateArray();
	if (rank > 1) {
		for (int i = 0; i < dims[rank - 1]; i++) {
			cJSON_AddItemToArray(a, array_to_json(rank - 1, dims, fa, save));
		}
	} else if (rank == 1) {
		for (int i = 0; i < (*fa)->nr_values; i++) {
			cJSON *item = value_to_json((*fa)->values[i].value, (*fa)->values[i].type, save);
			cJSON_AddItemToArray(a, item);
		}
		(*fa)++;
	}
	return a;
}

static cJSON *value_to_json(int32_t value, enum ain_data_type type, struct gsave *save)
{
	switch (type) {
	case AIN_BOOL:
		return cJSON_CreateBool(value);
	case AIN_INT:
		return cJSON_CreateNumber(value);
	case AIN_STRING:
		return to_json_string(save->strings[value]->text);
	case AIN_LONG_INT:
		return create_number_wrapper("lint", value);
	case AIN_VOID:
		return create_number_wrapper("void", value);
	case AIN_FUNC_TYPE:
		return create_number_wrapper("functype", value);
	case AIN_FLOAT:
		{
			union { int32_t i; float f; } v = { .i = value };
			return create_number_wrapper("float", v.f);
		}
	case AIN_REF_TYPE:
		return create_number_wrapper("ref", type);
	case AIN_STRUCT:
		{
			struct gsave_record *r = &save->records[value];
			if (r->type != GSAVE_RECORD_STRUCT)
				ERROR("unexpected type in records table: %d", r->type);
			cJSON *o = cJSON_CreateObject();
			cJSON_AddItemToObjectCS(o, "@type", to_json_string(r->struct_name));
			for (int i = 0; i < r->nr_indices; i++) {
				struct gsave_keyval *kv = &save->keyvals[r->indices[i]];
				char *key = conv_output(kv->name);
				cJSON_AddItemToObject(o, key, value_to_json(kv->value, kv->type, save));
				free(key);
			}
			return o;
		}
	case AIN_ARRAY_TYPE:
		{
			struct gsave_array *a = &save->arrays[value];
			cJSON *o = cJSON_CreateObject();
			cJSON_AddNumberToObject(o, "array_rank", a->rank);
			cJSON_AddNumberToObject(o, "type", type);
			if (a->rank < 0) {
				cJSON_AddItemToObjectCS(o, "value", cJSON_CreateNull());
				return o;
			}
			cJSON *dims = cJSON_CreateArray();
			for (int i = 0; i < a->rank; i++)
				cJSON_AddItemToArray(dims, cJSON_CreateNumber(a->dimensions[i]));
			cJSON_AddItemToObjectCS(o, "dimensions", dims);
			struct gsave_flat_array *fa = a->flat_arrays;
			cJSON_AddItemToObjectCS(o, "values", array_to_json(a->rank, a->dimensions, &fa, save));
			assert(fa == a->flat_arrays + a->nr_flat_arrays);
			return o;
		}
	default:
		ALICE_ERROR("Unhandled value type: %d", type);
	}
}

static cJSON *gsave_to_json(struct gsave *save)
{
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "save_type", "global_save");
	cJSON_AddItemToObjectCS(root, "key", to_json_string(save->key));
	cJSON_AddNumberToObject(root, "uk1", save->uk1);
	cJSON_AddNumberToObject(root, "version", save->version);
	cJSON_AddNumberToObject(root, "uk2", save->uk2);
	cJSON_AddNumberToObject(root, "num_ain_globals", save->nr_ain_globals);
	if (save->version >= 5)
		cJSON_AddItemToObjectCS(root, "group", to_json_string(save->group));

	cJSON *globals = cJSON_CreateArray();
	cJSON_AddItemToObjectCS(root, "globals", globals);
	for (struct gsave_global *g = save->globals; g < save->globals + save->nr_globals; g++) {
		cJSON *global = cJSON_CreateObject();
		cJSON_AddItemToArray(globals, global);
		cJSON_AddItemToObjectCS(global, "name", to_json_string(g->name));
		cJSON_AddItemToObjectCS(global, "value", value_to_json(g->value, g->type, save));
		cJSON_AddNumberToObject(global, "unknown", g->unknown);
	}

	return root;
}

int command_asd_dump(int argc, char *argv[])
{
	bool decode = false;
	char *output_file = NULL;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_asd_dump);
		if (c == -1)
			break;

		switch (c) {
		case 'd':
		case LOPT_DECODE:
			decode = true;
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
		USAGE_ERROR(&cmd_asd_dump, "Wrong number of arguments.");
	}
	enum savefile_error error;
	struct savefile *save = savefile_read(argv[0], &error);
	if (!save)
		ALICE_ERROR("Cannot read '%s': %s", argv[0], savefile_strerror(error));

	FILE *out = alice_open_output_file(output_file);

	if (decode) {
		if (fwrite(save->buf, save->len, 1, out) != 1)
			ALICE_ERROR("Error writing to file: %s", strerror(errno));
		savefile_free(save);
		return 0;
	}

	if (!memcmp(save->buf, "RSM\0", 4))
		ALICE_ERROR("%s: Resume save is not supported yet.", argv[0]);

	struct gsave *gsave = xcalloc(1, sizeof(struct gsave));
	error = gsave_parse(save->buf, save->len, gsave);
	if (error != SAVEFILE_SUCCESS)
		ALICE_ERROR("Cannot parse '%s': %s", argv[0], savefile_strerror(error));
	cJSON *json = gsave_to_json(gsave);
	cJSON_AddBoolToObject(json, "encrypted", save->encrypted);
	cJSON_AddNumberToObject(json, "compression_level", save->compression_level);
	char *text = cJSON_Print(json);
	if (fputs(text, out) == EOF)
		ALICE_ERROR("Error writing to file: %s", strerror(errno));

	free(text);
	cJSON_Delete(json);
	gsave_free(gsave);
	savefile_free(save);
	return 0;
}

struct command cmd_asd_dump = {
	.name = "dump",
	.usage = "[options...] <input-file>",
	.description = "Dump the contents of a save file",
	.parent = &cmd_asd,
	.fun = command_asd_dump,
	.options = {
		{ "decode", 'd', "Dump decrypted and uncompressed save file", no_argument, LOPT_DECODE },
		{ "output", 'o', "Specify the output file path",              required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
