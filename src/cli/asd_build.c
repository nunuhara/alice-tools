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
	if (!fa->nr_values)
		ALICE_ERROR("empty flat_array");
	fa->type = fa->values[0].type;
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
		ALICE_ERROR("unexpected json object: %s", cJSON_PrintUnformatted(v));

	cJSON *struct_name = cJSON_GetObjectItem(v, "@type");
	if (cJSON_IsString(struct_name)) {
		int nr_fields = cJSON_GetArraySize(v) - 1;
		struct gsave_record rec = {
			.type = GSAVE_RECORD_STRUCT,
			.struct_name = conv_output(struct_name->valuestring),
			.nr_indices = nr_fields,
			.indices = xcalloc(nr_fields, sizeof(int32_t)),
		};
		if (save->version >= 7) {
			rec.struct_index = gsave_get_struct_def(save, rec.struct_name);
			if (rec.struct_index < 0)
				ALICE_ERROR("no struct definition for %s", struct_name->valuestring);
		}
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
	ALICE_ERROR("unexpected json object: %s", cJSON_PrintUnformatted(v));
}

static struct gsave *json_to_gsave(cJSON *root)
{
	struct gsave *save = xcalloc(1, sizeof(struct gsave));
	save->key = json_get_string(root, "key");
	save->uk1 = json_get_int(root, "uk1");
	save->version = json_get_int(root, "version");
	save->uk2 = json_get_int(root, "uk2");
	save->nr_ain_globals = json_get_int(root, "num_ain_globals");
	if (save->version >= 5)
		save->group = json_get_string(root, "group");

	if (save->version >= 7) {
		cJSON *struct_defs = json_get_array(root, "struct_defs");
		save->nr_struct_defs = cJSON_GetArraySize(struct_defs);
		save->struct_defs = xcalloc(save->nr_struct_defs, sizeof(struct gsave_struct_def));
		int i;
		cJSON *o;
		cJSON_ArrayForEachIndex(i, o, struct_defs) {
			struct gsave_struct_def *sd = &save->struct_defs[i];
			sd->name = json_get_string(o, "name");
			cJSON *fields = json_get_array(o, "fields");
			sd->nr_fields = cJSON_GetArraySize(fields);
			sd->fields = xcalloc(sd->nr_fields, sizeof(struct gsave_field_def));
			int j;
			cJSON *f;
			cJSON_ArrayForEachIndex(j, f, fields) {
				sd->fields[j].type = json_get_int(f, "type");
				sd->fields[j].name = json_get_string(f, "name");
			}
		}
	}

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
		if (save->version <= 5)
			save->globals[i].unknown = json_get_int(o, "unknown");
	}
	return save;
}

static int32_t *json_to_int_array(cJSON *array, int32_t *n_out)
{
	int n = cJSON_GetArraySize(array);
	int32_t *a = xcalloc(n, sizeof(int32_t));
	int i;
	cJSON *item;
	cJSON_ArrayForEachIndex(i, item, array) {
		if (!cJSON_IsNumber(item))
			ALICE_ERROR("Non-number in %s", array->string);
		a[i] = item->valueint;
	}
	*n_out = n;
	return a;
}

static char **json_to_string_array(cJSON *array, int32_t *n_out)
{
	int n = cJSON_GetArraySize(array);
	char **a = xcalloc(n, sizeof(char*));
	int i;
	cJSON *item;
	cJSON_ArrayForEachIndex(i, item, array) {
		if (!cJSON_IsString(item))
			ALICE_ERROR("Non-string in %s", array->string);
		a[i] = conv_output(item->valuestring);
	}
	*n_out = n;
	return a;
}

static struct rsave_symbol json_to_rsave_symbol(cJSON *json)
{
	if (cJSON_IsString(json))
		return (struct rsave_symbol) { .name = conv_output(json->valuestring) };
	else if (cJSON_IsNumber(json))
		return (struct rsave_symbol) { .id = json->valueint };
	else
		ALICE_ERROR("Expected string or number for '%s'", json->string);
}

static void json_to_rsave_call_frame(cJSON *f, struct rsave_call_frame *dest)
{
	dest->type = json_get_int(f, "type");
	dest->local_ptr = json_get_int(f, "local_ptr");
	dest->struct_ptr = dest->type == RSAVE_METHOD_CALL ?
		json_get_int(f, "struct_ptr") : -1;
}

static void json_to_rsave_return_record(cJSON *f, struct rsave_return_record *dest)
{
	if (cJSON_IsNull(f)) {
		dest->return_addr = -1;
		return;
	}
	dest->return_addr = json_get_int(f, "return_addr");
	dest->caller_func = json_get_string(f, "caller_func");
	dest->local_addr = json_get_int(f, "local_addr");
	dest->crc = json_get_int(f, "crc");
}

static struct rsave_heap_frame *json_to_rsave_frame(int32_t version, cJSON *json, enum rsave_heap_tag type)
{
	int nr_slots;
	int32_t *slots = json_to_int_array(json_get_array(json, "slots"), &nr_slots);
	struct rsave_heap_frame *f = xmalloc(sizeof(struct rsave_heap_frame) + nr_slots * sizeof(int32_t));
	f->tag = type;
	f->ref = json_get_int(json, "ref");
	if (version >= 9)
		f->seq = json_get_int(json, "seq");
	f->func = json_to_rsave_symbol(cJSON_GetObjectItem(json, "func"));
	f->types = json_to_int_array(json_get_array(json, "types"), &f->nr_types);
	if (type == RSAVE_LOCALS && version >= 9)
		f->struct_ptr = json_get_int(json, "struct_ptr");
	f->nr_slots = nr_slots;
	memcpy(f->slots, slots, nr_slots * sizeof(int32_t));
	free(slots);
	return f;
}

static struct rsave_heap_string *json_to_rsave_string(int32_t version, cJSON *json)
{
	cJSON *text = cJSON_GetObjectItem(json, "text");
	char *buf;
	int len;
	if (cJSON_IsString(text)) {
		buf = conv_output(text->valuestring);
		len = strlen(buf) + 1;
	} else if (cJSON_IsArray(text)) {
		len = cJSON_GetArraySize(text);
		buf = xmalloc(len);
		int i;
		cJSON *e;
		cJSON_ArrayForEachIndex(i, e, text) {
			buf[i] = e->valueint;
		}
	} else {
		ALICE_ERROR("No valid 'text' field.");
	}

	struct rsave_heap_string *s = xmalloc(sizeof(struct rsave_heap_string) + len);
	s->tag = RSAVE_STRING;
	s->ref = json_get_int(json, "ref");
	if (version >= 9)
		s->seq = json_get_int(json, "seq");
	s->uk = json_get_int(json, "uk");
	s->len = len;
	memcpy(s->text, buf, len);
	free(buf);
	return s;
}

static struct rsave_heap_array *json_to_rsave_array(int32_t version, cJSON *json)
{
	int nr_slots;
	int32_t *slots = json_to_int_array(json_get_array(json, "slots"), &nr_slots);
	struct rsave_heap_array *a = xmalloc(sizeof(struct rsave_heap_array) + nr_slots * sizeof(int32_t));
	a->tag = RSAVE_ARRAY;
	a->ref = json_get_int(json, "ref");
	if (version >= 9)
		a->seq = json_get_int(json, "seq");
	a->rank_minus_1 = json_get_int(json, "rank_minus_1");
	a->data_type = json_get_int(json, "data_type");
	a->struct_type = json_to_rsave_symbol(cJSON_GetObjectItem(json, "struct_type"));
	a->root_rank = json_get_int(json, "root_rank");
	a->is_not_empty = json_get_int(json, "is_not_empty");
	a->nr_slots = nr_slots;
	memcpy(a->slots, slots, nr_slots * sizeof(int32_t));
	free(slots);
	return a;
}

static struct rsave_heap_struct *json_to_rsave_struct(int32_t version, cJSON *json)
{
	int nr_slots;
	int32_t *slots = json_to_int_array(json_get_array(json, "slots"), &nr_slots);
	struct rsave_heap_struct *s = xmalloc(sizeof(struct rsave_heap_struct) + nr_slots * sizeof(int32_t));
	s->tag = RSAVE_STRUCT;
	s->ref = json_get_int(json, "ref");
	if (version >= 9)
		s->seq = json_get_int(json, "seq");
	s->ctor = json_to_rsave_symbol(cJSON_GetObjectItem(json, "ctor"));
	s->dtor = json_to_rsave_symbol(cJSON_GetObjectItem(json, "dtor"));
	s->uk = json_get_int(json, "uk");
	s->struct_type = json_to_rsave_symbol(cJSON_GetObjectItem(json, "struct_type"));
	s->types = json_to_int_array(json_get_array(json, "types"), &s->nr_types);
	s->nr_slots = nr_slots;
	memcpy(s->slots, slots, nr_slots * sizeof(int32_t));
	free(slots);
	return s;
}

static struct rsave_heap_delegate *json_to_rsave_delegate(int32_t version, cJSON *json)
{
	int nr_slots;
	int32_t *slots = json_to_int_array(json_get_array(json, "slots"), &nr_slots);
	struct rsave_heap_delegate *d = xmalloc(sizeof(struct rsave_heap_delegate) + nr_slots * sizeof(int32_t));
	d->tag = RSAVE_DELEGATE;
	d->ref = json_get_int(json, "ref");
	if (version >= 9)
		d->seq = json_get_int(json, "seq");
	d->nr_slots = nr_slots;
	memcpy(d->slots, slots, nr_slots * sizeof(int32_t));
	free(slots);
	return d;
}

static void *json_to_heap_obj(int32_t version, cJSON *item)
{
	if (cJSON_IsNull(item))
		return rsave_null;
	cJSON *type = cJSON_GetObjectItem(item, "type");
	if (!strcmp(type->valuestring, "globals"))
		return json_to_rsave_frame(version, item, RSAVE_GLOBALS);
	if (!strcmp(type->valuestring, "locals"))
		return json_to_rsave_frame(version, item, RSAVE_LOCALS);
	if (!strcmp(type->valuestring, "string"))
		return json_to_rsave_string(version, item);
	if (!strcmp(type->valuestring, "array"))
		return json_to_rsave_array(version, item);
	if (!strcmp(type->valuestring, "struct"))
		return json_to_rsave_struct(version, item);
	if (!strcmp(type->valuestring, "delegate"))
		return json_to_rsave_delegate(version, item);
	ALICE_ERROR("unknown heap object type %s", type->valuestring);
}

static struct rsave *json_to_rsave(cJSON *root)
{
	int i;
	cJSON *item;
	struct rsave *save = xcalloc(1, sizeof(struct rsave));
	save->version = json_get_int(root, "version");
	save->key = json_get_string(root, "key");
	if (save->version >= 7) {
		cJSON *comments = json_get_array(root, "comments");
		save->comments = json_to_string_array(comments, &save->nr_comments);
	}
	if (cJSON_IsTrue(cJSON_GetObjectItem(root, "comments_only"))) {
		save->comments_only = true;
		return save;
	}

	json_to_rsave_return_record(cJSON_GetObjectItem(root, "ip"), &save->ip);
	save->uk1 = json_get_int(root, "uk1");

	save->stack = json_to_int_array(json_get_array(root, "stack"), &save->stack_size);

	cJSON *call_frames = json_get_array(root, "call_frames");
	save->nr_call_frames = cJSON_GetArraySize(call_frames);
	save->call_frames = xcalloc(save->nr_call_frames, sizeof(struct rsave_call_frame));
	cJSON_ArrayForEachIndex(i, item, call_frames) {
		json_to_rsave_call_frame(item, &save->call_frames[i]);
	}

	cJSON *return_records = json_get_array(root, "return_records");
	save->nr_return_records = cJSON_GetArraySize(return_records);
	save->return_records = xcalloc(save->nr_return_records, sizeof(struct rsave_return_record));
	cJSON_ArrayForEachIndex(i, item, return_records) {
		json_to_rsave_return_record(item, &save->return_records[i]);
	}

	save->uk2 = json_get_int(root, "uk2");
	save->uk3 = json_get_int(root, "uk3");
	save->uk4 = json_get_int(root, "uk4");
	if (save->version >= 9)
		save->next_seq = json_get_int(root, "next_seq");

	cJSON *heap = json_get_array(root, "heap");
	save->nr_heap_objs = cJSON_GetArraySize(heap);
	save->heap = xcalloc(save->nr_heap_objs, sizeof(void*));
	cJSON_ArrayForEachIndex(i, item, heap) {
		save->heap[i] = json_to_heap_obj(save->version, item);
	}

	if (save->version >= 6) {
		cJSON *func_names = json_get_array(root, "func_names");
		save->func_names = json_to_string_array(func_names, &save->nr_func_names);
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

	cJSON *v = cJSON_GetObjectItem(json, "encrypted");
	bool encrypt = cJSON_IsBool(v) ? v->valueint : true;
	v = cJSON_GetObjectItem(json, "compression_level");
	int compression_level = cJSON_IsNumber(v) ? v->valueint : 9;

	cJSON *save_type = cJSON_GetObjectItem(json, "save_type");
	if (!cJSON_IsString(save_type)) {
		ALICE_ERROR("not a save json");
	}
	enum savefile_error error;
	if (!strcmp(save_type->valuestring, "global_save")) {
		struct gsave *gs = json_to_gsave(json);
		error = gsave_write(gs, out, encrypt, compression_level);
		gsave_free(gs);
	} else if (!strcmp(save_type->valuestring, "resume_save")) {
		struct rsave *rs = json_to_rsave(json);
		error = rsave_write(rs, out, encrypt, compression_level);
		rsave_free(rs);
	} else {
		ALICE_ERROR("unrecognized save_type '%s'", save_type->valuestring);
	}
	if (error != SAVEFILE_SUCCESS)
		ALICE_ERROR("Error writing output: %s", savefile_strerror(error));

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
