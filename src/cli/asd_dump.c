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

static int cb_get_number(int i, void *data)
{
	return ((int32_t *)data)[i];
}

static cJSON *int_array_to_json(int32_t *nums, int count)
{
	return cJSON_CreateIntArray_cb(count, cb_get_number, nums);
}

static cJSON *cb_get_string(int i, void *data)
{
	return to_json_string(((char **)data)[i]);
}

static cJSON *string_array_to_json(char **strs, int count)
{
	return cJSON_CreateArray_cb(count, cb_get_string, strs);
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
		if (value == GSAVE7_EMPTY_STRING)
			return cJSON_CreateString("");
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
			cJSON *o = cJSON_CreateObject();
			if (save->version <= 5) {
				if (r->type != GSAVE_RECORD_STRUCT)
					ALICE_ERROR("unexpected type in records table: %d", r->type);
				cJSON_AddItemToObjectCS(o, "@type", to_json_string(r->struct_name));
				for (int i = 0; i < r->nr_indices; i++) {
					struct gsave_keyval *kv = &save->keyvals[r->indices[i]];
					char *key = conv_output(kv->name);
					cJSON_AddItemToObject(o, key, value_to_json(kv->value, kv->type, save));
					free(key);
				}
			} else {
				if (r->struct_index < 0)
					ALICE_ERROR("unexpected type in records table: %d", r->type);
				struct gsave_struct_def *sd = &save->struct_defs[r->struct_index];
				cJSON_AddItemToObjectCS(o, "@type", to_json_string(sd->name));
				if (r->nr_indices != sd->nr_fields)
					ALICE_ERROR("record %d has %d fields, but struct %d has %d fields", value, r->nr_indices, r->struct_index, sd->nr_fields);
				for (int i = 0; i < r->nr_indices; i++) {
					struct gsave_keyval *kv = &save->keyvals[r->indices[i]];
					struct gsave_field_def *fd = &sd->fields[i];
					char *key = conv_output(fd->name);
					cJSON_AddItemToObject(o, key, value_to_json(kv->value, fd->type, save));
					free(key);
				}
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
			cJSON_AddItemToObjectCS(o, "dimensions", int_array_to_json(a->dimensions, a->rank));
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
		if (save->version <= 5)
			cJSON_AddNumberToObject(global, "unknown", g->unknown);
	}

	if (save->version >= 7) {
		cJSON *struct_defs = cJSON_CreateArray();
		cJSON_AddItemToObjectCS(root, "struct_defs", struct_defs);
		for (struct gsave_struct_def *sd = save->struct_defs; sd < save->struct_defs + save->nr_struct_defs; sd++) {
			cJSON *struct_def = cJSON_CreateObject();
			cJSON_AddItemToArray(struct_defs, struct_def);
			cJSON_AddItemToObjectCS(struct_def, "name", to_json_string(sd->name));
			cJSON *fields = cJSON_CreateArray();
			cJSON_AddItemToObjectCS(struct_def, "fields", fields);
			for (struct gsave_field_def *fd = sd->fields; fd < sd->fields + sd->nr_fields; fd++) {
				cJSON *field = cJSON_CreateObject();
				cJSON_AddItemToArray(fields, field);
				cJSON_AddNumberToObject(field, "type", fd->type);
				cJSON_AddItemToObjectCS(field, "name", to_json_string(fd->name));
			}
		}
	}

	return root;
}

static cJSON *rsave_symbol_to_json(struct rsave_symbol *sym)
{
	if (sym->name)
		return to_json_string(sym->name);
	return cJSON_CreateNumber(sym->id);
}

static cJSON *rsave_call_frame_to_json(struct rsave_call_frame *f)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddNumberToObject(o, "type", f->type);
	cJSON_AddNumberToObject(o, "local_ptr", f->local_ptr);
	if (f->type == RSAVE_METHOD_CALL)
		cJSON_AddNumberToObject(o, "struct_ptr", f->struct_ptr);
	return o;
}

static cJSON *rsave_return_record_to_json(struct rsave_return_record *f)
{
	if (f->return_addr == -1)
		return cJSON_CreateNull();
	cJSON *o = cJSON_CreateObject();
	cJSON_AddNumberToObject(o, "return_addr", f->return_addr);
	cJSON_AddItemToObjectCS(o, "caller_func", to_json_string(f->caller_func));
	cJSON_AddNumberToObject(o, "local_addr", f->local_addr);
	cJSON_AddNumberToObject(o, "crc", f->crc);
	return o;
}

static cJSON *rsave_frame_to_json(int32_t version, struct rsave_heap_frame *f)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "type", f->tag == RSAVE_GLOBALS ? "globals" : "locals");
	cJSON_AddNumberToObject(o, "ref", f->ref);
	if (version >= 9)
		cJSON_AddNumberToObject(o, "seq", f->seq);
	cJSON_AddItemToObjectCS(o, "func", rsave_symbol_to_json(&f->func));

	cJSON *types = cJSON_CreateArray();
	for (int i = 0; i < f->nr_types; i++)
		cJSON_AddItemToArray(types, cJSON_CreateNumber(f->types[i]));
	cJSON_AddItemToObjectCS(o, "types", types);

	if (f->tag == RSAVE_LOCALS && version >= 9)
		cJSON_AddNumberToObject(o, "struct_ptr", f->struct_ptr);
	cJSON_AddItemToObjectCS(o, "slots", int_array_to_json(f->slots, f->nr_slots));
	return o;
}

static cJSON *rsave_string_to_json(int32_t version, struct rsave_heap_string *s)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "type", "string");
	cJSON_AddNumberToObject(o, "ref", s->ref);
	if (version >= 9)
		cJSON_AddNumberToObject(o, "seq", s->seq);
	cJSON_AddNumberToObject(o, "uk", s->uk);
	if (strlen(s->text) + 1 == (size_t)s->len) {
		cJSON_AddItemToObjectCS(o, "text", to_json_string(s->text));
	} else {
		// Serialize as a byte array.
		cJSON *bytes = cJSON_CreateArray();
		for (int i = 0; i < s->len; i++)
			cJSON_AddItemToArray(bytes, cJSON_CreateNumber(s->text[i]));
		cJSON_AddItemToObjectCS(o, "text", bytes);
	}
	return o;
}

static cJSON *rsave_array_to_json(int32_t version, struct rsave_heap_array *a)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "type", "array");
	cJSON_AddNumberToObject(o, "ref", a->ref);
	if (version >= 9)
		cJSON_AddNumberToObject(o, "seq", a->seq);
	cJSON_AddNumberToObject(o, "rank_minus_1", a->rank_minus_1);
	cJSON_AddNumberToObject(o, "data_type", a->data_type);
	cJSON_AddItemToObjectCS(o, "struct_type", rsave_symbol_to_json(&a->struct_type));
	cJSON_AddNumberToObject(o, "root_rank", a->root_rank);
	cJSON_AddNumberToObject(o, "is_not_empty", a->is_not_empty);
	cJSON_AddItemToObjectCS(o, "slots", int_array_to_json(a->slots, a->nr_slots));
	return o;
}

static cJSON *rsave_struct_to_json(int32_t version, struct rsave_heap_struct *s)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "type", "struct");
	cJSON_AddNumberToObject(o, "ref", s->ref);
	if (version >= 9)
		cJSON_AddNumberToObject(o, "seq", s->seq);
	cJSON_AddItemToObjectCS(o, "ctor", rsave_symbol_to_json(&s->ctor));
	cJSON_AddItemToObjectCS(o, "dtor", rsave_symbol_to_json(&s->dtor));
	cJSON_AddNumberToObject(o, "uk", s->uk);
	cJSON_AddItemToObjectCS(o, "struct_type", rsave_symbol_to_json(&s->struct_type));

	cJSON *types = cJSON_CreateArray();
	for (int i = 0; i < s->nr_types; i++)
		cJSON_AddItemToArray(types, cJSON_CreateNumber(s->types[i]));
	cJSON_AddItemToObjectCS(o, "types", types);

	cJSON_AddItemToObjectCS(o, "slots", int_array_to_json(s->slots, s->nr_slots));
	return o;
}

static cJSON *rsave_delegate_to_json(int32_t version, struct rsave_heap_delegate *d)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "type", "delegate");
	cJSON_AddNumberToObject(o, "ref", d->ref);
	if (version >= 9)
		cJSON_AddNumberToObject(o, "seq", d->seq);
	cJSON_AddItemToObjectCS(o, "slots", int_array_to_json(d->slots, d->nr_slots));
	return o;
}

static cJSON *heap_obj_to_json(int i, void *data)
{
	struct rsave *save = data;
	void *obj = save->heap[i];
	enum rsave_heap_tag *tag = obj;
	switch (*tag) {
	case RSAVE_GLOBALS:
	case RSAVE_LOCALS:   return rsave_frame_to_json(save->version, obj);
	case RSAVE_STRING:   return rsave_string_to_json(save->version, obj);
	case RSAVE_ARRAY:    return rsave_array_to_json(save->version, obj);
	case RSAVE_STRUCT:   return rsave_struct_to_json(save->version, obj);
	case RSAVE_DELEGATE: return rsave_delegate_to_json(save->version, obj);
	case RSAVE_NULL:     return cJSON_CreateNull();
	}
	ALICE_ERROR("unknown heap object tag %d", *tag);
}

static cJSON *rsave_to_json(struct rsave *save)
{
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "save_type", "resume_save");
	cJSON_AddNumberToObject(root, "version", save->version);
	cJSON_AddItemToObjectCS(root, "key", to_json_string(save->key));

	if (save->version >= 7) {
		cJSON_AddItemToObjectCS(root, "comments", string_array_to_json(save->comments, save->nr_comments));
	}
	if (save->comments_only) {
		cJSON_AddBoolToObject(root, "comments_only", true);
		return root;
	}

	cJSON_AddItemToObjectCS(root, "ip", rsave_return_record_to_json(&save->ip));
	cJSON_AddNumberToObject(root, "uk1", save->uk1);
	cJSON_AddItemToObjectCS(root, "stack", int_array_to_json(save->stack, save->stack_size));

	cJSON *call_frames = cJSON_CreateArray();
	for (int i = 0; i < save->nr_call_frames; i++)
		cJSON_AddItemToArray(call_frames, rsave_call_frame_to_json(&save->call_frames[i]));
	cJSON_AddItemToObjectCS(root, "call_frames", call_frames);

	cJSON *return_records = cJSON_CreateArray();
	for (int i = 0; i < save->nr_return_records; i++)
		cJSON_AddItemToArray(return_records, rsave_return_record_to_json(&save->return_records[i]));
	cJSON_AddItemToObjectCS(root, "return_records", return_records);

	cJSON_AddNumberToObject(root, "uk2", save->uk2);
	cJSON_AddNumberToObject(root, "uk3", save->uk3);
	cJSON_AddNumberToObject(root, "uk4", save->uk4);
	if (save->version >= 9)
		cJSON_AddNumberToObject(root, "next_seq", save->next_seq);
	cJSON_AddItemToObjectCS(root, "heap", cJSON_CreateArray_cb(save->nr_heap_objs, heap_obj_to_json, save));
	cJSON_AddItemToObjectCS(root, "func_names", string_array_to_json(save->func_names, save->nr_func_names));
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

	cJSON *json;
	if (!memcmp(save->buf, "RSM\0", 4)) {
		struct rsave *rsave = xcalloc(1, sizeof(struct rsave));
		error = rsave_parse(save->buf, save->len, RSAVE_READ_ALL, rsave);
		if (error != SAVEFILE_SUCCESS)
			ALICE_ERROR("Cannot parse '%s': %s", argv[0], savefile_strerror(error));
		json = rsave_to_json(rsave);
		rsave_free(rsave);
	} else {
		struct gsave *gsave = xcalloc(1, sizeof(struct gsave));
		error = gsave_parse(save->buf, save->len, gsave);
		if (error != SAVEFILE_SUCCESS)
			ALICE_ERROR("Cannot parse '%s': %s", argv[0], savefile_strerror(error));
		json = gsave_to_json(gsave);
		gsave_free(gsave);
	}
	// Add some metadata about the envelope format.
	cJSON_AddBoolToObject(json, "encrypted", save->encrypted);
	cJSON_AddNumberToObject(json, "compression_level", save->compression_level);

	char *text = cJSON_Print(json);
	if (fputs(text, out) == EOF)
		ALICE_ERROR("Error writing to file: %s", strerror(errno));

	free(text);
	cJSON_Delete(json);
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
