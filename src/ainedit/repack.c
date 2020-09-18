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
#include <zlib.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/string.h"

struct ain_buffer {
	uint8_t *buf;
	size_t size;
	size_t ptr;
};

static void alloc_ainbuf(struct ain_buffer *out, size_t size)
{
	if (out->ptr + size < out->size)
		return;

	size_t new_size = out->ptr + size;
	while (out->size <= new_size)
		out->size *= 2;
	out->buf = xrealloc(out->buf, out->size);
}

static void _write_int32(uint8_t *buf, uint32_t v)
{
	buf[0] = (v & 0x000000FF);
	buf[1] = (v & 0x0000FF00) >> 8;
	buf[2] = (v & 0x00FF0000) >> 16;
	buf[3] = (v & 0xFF000000) >> 24;
}

static void write_int32(struct ain_buffer *out, uint32_t v)
{
	alloc_ainbuf(out, 4);
	out->buf[out->ptr++] = (v & 0x000000FF);
	out->buf[out->ptr++] = (v & 0x0000FF00) >> 8;
	out->buf[out->ptr++] = (v & 0x00FF0000) >> 16;
	out->buf[out->ptr++] = (v & 0xFF000000) >> 24;
}

static void write_bytes(struct ain_buffer *out, const uint8_t *bytes, size_t len)
{
	alloc_ainbuf(out, len);
	memcpy(out->buf + out->ptr, bytes, len);
	out->ptr += len;
}

static void write_string(struct ain_buffer *out, const char *s)
{
	size_t len = strlen(s);
	write_bytes(out, (const uint8_t*)s, len+1);
}

static void write_header(struct ain_buffer *out, const char *s)
{
	write_bytes(out, (const uint8_t*)s, 4);
}

static void write_variable_type(struct ain_buffer *out, struct ain *ain, struct ain_type *t)
{
	write_int32(out, t->data);
	write_int32(out, t->struc);
	write_int32(out, t->rank);

	if (ain_is_array_data_type(t->data)) {
		for (int i = 0; i < t->rank; i++) {
			write_variable_type(out, ain, &t->array_type[i]);
		}
	}
}

static void write_return_type(struct ain_buffer *out, struct ain *ain, struct ain_type *t)
{
	if (AIN_VERSION_GTE(ain, 11, 0)) {
		write_variable_type(out, ain, t);
		return;
	}

	write_int32(out, t->data);
	write_int32(out, t->struc);
}

static void write_variable(struct ain_buffer *out, struct ain *ain, struct ain_variable *v)
{
	write_string(out, v->name);
	if (AIN_VERSION_GTE(ain, 12, 0))
		write_string(out, v->name2);
	write_variable_type(out, ain, &v->type);
	if (AIN_VERSION_GTE(ain, 8, 0)) {
		write_int32(out, v->has_initval);
		if (v->has_initval) {
			switch (v->type.data) {
			case AIN_STRING:
				write_string(out, v->initval.s);
			case AIN_DELEGATE:
			case AIN_REF_TYPE:
			case AIN_ARRAY:
			case AIN_STRUCT:
				break;
			default:
				write_int32(out, v->initval.i);
			}
		}
	}
}

static void write_function(struct ain_buffer *out, struct ain *ain, struct ain_function *f)
{
	write_int32(out, f->address);
	write_string(out, f->name);
	if (ain->version > 0 && ain->version < 7)
		write_int32(out, f->is_label);
	write_return_type(out, ain, &f->return_type);
	write_int32(out, f->nr_args);
	write_int32(out, f->nr_vars);
	if (AIN_VERSION_GTE(ain, 11, 0))
		write_int32(out, f->is_lambda);
	if (ain->version > 0)
		write_int32(out, f->crc);
	for (int i = 0; i < f->nr_vars; i++) {
		write_variable(out, ain, &f->vars[i]);
	}
}

static void write_global(struct ain_buffer *out, struct ain *ain, struct ain_variable *g)
{
	write_string(out, g->name);
	if (AIN_VERSION_GTE(ain, 12, 0))
		write_string(out, g->name2);
	write_variable_type(out, ain, &g->type);
	if (AIN_VERSION_GTE(ain, 5, 0))
		write_int32(out, g->group_index);
}

static void write_initval(struct ain_buffer *out, possibly_unused struct ain *ain, struct ain_initval *v)
{
	write_int32(out, v->global_index);
	write_int32(out, v->data_type);
	if (v->data_type == AIN_STRING)
		write_string(out, v->string_value);
	else
		write_int32(out, v->int_value);
}

static void write_structure(struct ain_buffer *out, struct ain *ain, struct ain_struct *s)
{
	write_string(out, s->name);
	if (AIN_VERSION_GTE(ain, 11, 0)) {
		write_int32(out, s->nr_interfaces);
		for (int i = 0; i < s->nr_interfaces; i++) {
			write_int32(out, s->interfaces[i].struct_type);
			write_int32(out, s->interfaces[i].uk);
		}
	}
	write_int32(out, s->constructor);
	write_int32(out, s->destructor);
	write_int32(out, s->nr_members);
	for (int i = 0; i < s->nr_members; i++) {
		write_variable(out, ain, &s->members[i]);
	}
	if (AIN_VERSION_GTE(ain, 14, 1)) {
		write_int32(out, s->nr_vmethods);
		for (int i = 0; i < s->nr_vmethods; i++) {
			write_int32(out, s->vmethods[i]);
		}
	}
}

static void write_library(struct ain_buffer *out, struct ain *ain, struct ain_library *lib)
{
	write_string(out, lib->name);
	write_int32(out, lib->nr_functions);
	for (int i = 0; i < lib->nr_functions; i++) {
		write_string(out, lib->functions[i].name);
		if (AIN_VERSION_GTE(ain, 14, 0)) {
			write_variable_type(out, ain, &lib->functions[i].return_type);
		} else {
			write_int32(out, lib->functions[i].return_type.data);
		}
		write_int32(out, lib->functions[i].nr_arguments);
		for (int j = 0; j < lib->functions[i].nr_arguments; j++) {
			write_string(out, lib->functions[i].arguments[j].name);
			if (AIN_VERSION_GTE(ain, 14, 0)) {
				write_variable_type(out, ain, &lib->functions[i].arguments[j].type);
			} else {
				write_int32(out, lib->functions[i].arguments[j].type.data);
			}
		}
	}
}

static void write_switch(struct ain_buffer *out, possibly_unused struct ain *ain, struct ain_switch *s)
{
	write_int32(out, s->case_type);
	write_int32(out, s->default_address);
	write_int32(out, s->nr_cases);
	for (int i = 0; i < s->nr_cases; i++) {
		write_int32(out, s->cases[i].value);
		write_int32(out, s->cases[i].address);
	}
}

static void write_function_type(struct ain_buffer *out, struct ain *ain, struct ain_function_type *f)
{
	write_string(out, f->name);
	write_return_type(out, ain, &f->return_type);
	write_int32(out, f->nr_arguments);
	write_int32(out, f->nr_variables);
	for (int i = 0; i < f->nr_variables; i++) {
		write_variable(out, ain, &f->variables[i]);
	}
}

static void write_msg1_string(struct ain_buffer *out, possibly_unused struct ain *ain, struct string *msg)
{
	write_int32(out, msg->size);

	uint8_t *buf = xmalloc(msg->size);
	for (int i = 0; i < msg->size; i++) {
		buf[i] = msg->text[i];
		buf[i] += 0x60;
		buf[i] += (uint8_t)i;
	}

	write_bytes(out, buf, msg->size);
	free(buf);
}

static uint8_t *ain_flatten(struct ain *ain, size_t *len)
{
	struct ain_buffer out = {
		.buf = xmalloc(256),
		.size = 256,
		.ptr = 0
	};

	// VERS
	write_header(&out, "VERS");
	write_int32(&out, ain->version);
	// KEYC
	if (ain->KEYC.present) {
		write_header(&out, "KEYC");
		write_int32(&out, ain->keycode);
	}
	// CODE
	if (ain->CODE.present) {
		write_header(&out, "CODE");
		write_int32(&out, ain->code_size);
		write_bytes(&out, ain->code, ain->code_size);
	}
	// FUNC
	if (ain->FUNC.present) {
		write_header(&out, "FUNC");
		write_int32(&out, ain->nr_functions);
		for (int i = 0; i < ain->nr_functions; i++) {
			write_function(&out, ain, &ain->functions[i]);
		}
	}
	// GLOB
	if (ain->GLOB.present) {
		write_header(&out, "GLOB");
		write_int32(&out, ain->nr_globals);
		for (int i = 0; i < ain->nr_globals; i++) {
			write_global(&out, ain, &ain->globals[i]);
		}
	}
	// GSET
	if (ain->GSET.present) {
		write_header(&out, "GSET");
		write_int32(&out, ain->nr_initvals);
		for (int i = 0; i < ain->nr_initvals; i++) {
			write_initval(&out, ain, &ain->global_initvals[i]);
		}
	}
	// STRT
	if (ain->STRT.present) {
		write_header(&out, "STRT");
		write_int32(&out, ain->nr_structures);
		for (int i = 0; i < ain->nr_structures; i++) {
			write_structure(&out, ain, &ain->structures[i]);
		}
	}
	// MSG0
	if (ain->MSG0.present) {
		write_header(&out, "MSG0");
		write_int32(&out, ain->nr_messages);
		for (int i = 0; i < ain->nr_messages; i++) {
			write_bytes(&out, (uint8_t*)ain->messages[i]->text, ain->messages[i]->size+1);
		}
	}
	// MSG1
	if (ain->MSG1.present) {
		write_header(&out, "MSG1");
		write_int32(&out, ain->nr_messages);
		write_int32(&out, ain->msg1_uk);
		for (int i = 0; i < ain->nr_messages; i++) {
			write_msg1_string(&out, ain, ain->messages[i]);
		}
	}
	// MAIN
	if (ain->MAIN.present) {
		write_header(&out, "MAIN");
		write_int32(&out, ain->main);
	}
	// MSGF
	if (ain->MSGF.present) {
		write_header(&out, "MSGF");
		write_int32(&out, ain->msgf);
	}
	// HLL0
	if (ain->HLL0.present) {
		write_header(&out, "HLL0");
		write_int32(&out, ain->nr_libraries);
		for (int i = 0; i < ain->nr_libraries; i++) {
			write_library(&out, ain, &ain->libraries[i]);
		}
	}
	// SWI0
	if (ain->SWI0.present) {
		write_header(&out, "SWI0");
		write_int32(&out, ain->nr_switches);
		for (int i = 0; i < ain->nr_switches; i++) {
			write_switch(&out, ain, &ain->switches[i]);
		}
	}
	// GVER
	if (ain->GVER.present) {
		write_header(&out, "GVER");
		write_int32(&out, ain->game_version);
	}
	// STR0
	if (ain->STR0.present) {
		write_header(&out, "STR0");
		write_int32(&out, ain->nr_strings);
		for (int i = 0; i < ain->nr_strings; i++) {
			write_bytes(&out, (uint8_t*)ain->strings[i]->text, ain->strings[i]->size+1);
		}
	}
	// FNAM
	if (ain->FNAM.present) {
		write_header(&out, "FNAM");
		write_int32(&out, ain->nr_filenames);
		for (int i = 0; i < ain->nr_filenames; i++) {
			write_string(&out, ain->filenames[i]);
		}
	}
	// OJMP
	if (ain->OJMP.present) {
		write_header(&out, "OJMP");
		write_int32(&out, ain->ojmp);
	}
	// FNCT
	if (ain->FNCT.present) {
		write_header(&out, "FNCT");
		write_int32(&out, ain->fnct_size);
		write_int32(&out, ain->nr_function_types);
		for (int i = 0; i < ain->nr_function_types; i++) {
			write_function_type(&out, ain, &ain->function_types[i]);
		}
	}
	// DELG
	if (ain->DELG.present) {
		write_header(&out, "DELG");
		write_int32(&out, ain->delg_size);
		write_int32(&out, ain->nr_delegates);
		for (int i = 0; i < ain->nr_delegates; i++) {
			write_function_type(&out, ain, &ain->delegates[i]);
		}
	}
	// OBJG
	if (ain->OBJG.present) {
		write_header(&out, "OBJG");
		write_int32(&out, ain->nr_global_groups);
		for (int i = 0; i < ain->nr_global_groups; i++) {
			write_string(&out, ain->global_group_names[i]);
		}
	}
	// ENUM
	if (ain->ENUM.present) {
		write_header(&out, "ENUM");
		write_int32(&out, ain->nr_enums);
		for (int i = 0; i < ain->nr_enums; i++) {
			write_string(&out, ain->enums[i].name);
		}
	}

	*len = out.ptr;
	return out.buf;
}

static uint8_t *ain_compress(uint8_t *buf, size_t *len)
{
	unsigned long dst_len = *len * 1.001 + 12;
	uint8_t *dst = xmalloc(dst_len + 16);

	memcpy(dst, "AI2\0\0\0\0", 8);
	_write_int32(dst+8, *len);

	int r = compress2(dst+16, &dst_len, buf, *len, 1);
	if (r != Z_OK) {
		ERROR("compress failed");
	}
	free(buf);

	_write_int32(dst+12, dst_len);
	*len = dst_len + 16;
	return dst;
}

void ain_write(const char *filename, struct ain *ain)
{
	size_t len;
	uint8_t *buf = ain_flatten(ain, &len);

	if (ain->version <= 5)
		ain_decrypt(buf, len); // NOTE: this actually encrypts the buffer
	else
		buf = ain_compress(buf, &len);

	FILE *out = fopen(filename, "wb");
	if (!out)
		ERROR("Failed to open '%s': %s", filename, strerror(errno));
	if (fwrite(buf, len, 1, out) != 1)
		ERROR("Failed to write to '%s': %s", filename, strerror(errno));
	if (fclose(out))
		ERROR("Failed to close '%s': %s", filename, strerror(errno));

	free(buf);
}
