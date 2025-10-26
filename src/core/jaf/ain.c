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
#include <assert.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/jaf.h"

void jaf_define_struct(struct ain *ain, struct jaf_block_item *def)
{
	assert(def->kind == JAF_DECL_STRUCT);
	if (!def->struc.name)
		JAF_ERROR(def, "Anonymous structs not supported");

	char *name = conv_output(def->struc.name->text);
	if (ain_get_struct(ain, name) >= 0) {
		JAF_ERROR(def, "Redefining structs not supported");
	}

	def->struc.struct_no = ain_add_struct(ain, name);
	free(name);
}

void jaf_define_interface(struct ain *ain, struct jaf_block_item *def)
{
	assert(def->kind == JAF_DECL_INTERFACE);
	assert(def->struc.name);

	char *name = conv_output(def->struc.name->text);
	int iface_no = ain_get_struct(ain, name);
	if (iface_no < 0) {
		def->struc.struct_no = ain_add_struct(ain, name);
		ain->structures[def->struc.struct_no].is_interface = true;
	} else {
		def->struc.struct_no = iface_no;
	}
	free(name);
}

void jaf_define_functype(struct ain *ain, struct jaf_block_item *item)
{
	struct jaf_fundecl *decl = &item->fun;
	char *name = conv_output(jaf_name_collapse(ain, &decl->name)->text);
	if (ain_get_functype(ain, name) >= 0)
		JAF_ERROR(item, "Multiple definitions of function type '%s'", name);
	decl->func_no = ain_add_functype(ain, name);
	free(name);
}

void jaf_define_delegate(struct ain *ain, struct jaf_block_item *item)
{
	struct jaf_fundecl *decl = &item->fun;
	char *name = conv_output(jaf_name_collapse(ain, &decl->name)->text);
	if (ain_get_delegate(ain, name) >= 0)
		JAF_ERROR(item, "Multiple definitions of delegate '%s'", name);
	decl->func_no = ain_add_delegate(ain, name);
	free(name);
}

enum ain_data_type jaf_to_ain_simple_type(enum jaf_type type)
{
	switch (type) {
	case JAF_VOID:      return AIN_VOID;
	case JAF_INT:       return AIN_INT;
	case JAF_BOOL:      return AIN_BOOL;
	case JAF_FLOAT:     return AIN_FLOAT;
	case JAF_LONG_INT:  return AIN_LONG_INT;
	case JAF_STRING:    return AIN_STRING;
	case JAF_STRUCT:    return AIN_STRUCT;
	case JAF_IFACE:     return AIN_IFACE;
	case JAF_ENUM:      return AIN_ENUM;
	case JAF_ARRAY:     _COMPILER_ERROR(NULL, -1, "Invalid array type specifier");
	case JAF_WRAP:      return AIN_WRAP;
	case JAF_HLL_PARAM: return AIN_HLL_PARAM;
	case JAF_HLL_FUNC_71: return AIN_HLL_FUNC_71;
	case JAF_HLL_FUNC:  return AIN_HLL_FUNC;
	case JAF_IMAIN_SYSTEM: return AIN_IMAIN_SYSTEM;
	case JAF_DELEGATE:  return AIN_DELEGATE;
	case JAF_TYPEDEF:   _COMPILER_ERROR(NULL, -1, "Unresolved typedef");
	case JAF_FUNCTYPE:  return AIN_FUNC_TYPE;
	}
	_COMPILER_ERROR(NULL, -1, "Unknown type: %d", type);
}

static enum ain_data_type jaf_to_ain_data_type(struct ain *ain, struct jaf_type_specifier *type)
{
	if (type->qualifiers & JAF_QUAL_REF && type->type == JAF_ARRAY) {
		if (AIN_VERSION_GTE(ain, 11, 0)) {
			return AIN_REF_ARRAY;
		}
		switch (type->array_type->type) {
		case JAF_VOID:      _COMPILER_ERROR(NULL, -1, "void ref array type");
		case JAF_INT:       return AIN_REF_ARRAY_INT;
		case JAF_BOOL:      return AIN_REF_ARRAY_BOOL;
		case JAF_FLOAT:     return AIN_REF_ARRAY_FLOAT;
		case JAF_LONG_INT:  return AIN_REF_ARRAY_LONG_INT;
		case JAF_STRING:    return AIN_REF_ARRAY_STRING;
		case JAF_STRUCT:    return AIN_REF_ARRAY_STRUCT;
		case JAF_IFACE:     _COMPILER_ERROR(NULL, -1, "Invalid interface type specifier");
		case JAF_ENUM:      _COMPILER_ERROR(NULL, -1, "Enums not supported");
		case JAF_ARRAY:     _COMPILER_ERROR(NULL, -1, "Invalid array type specifier");
		case JAF_WRAP:      _COMPILER_ERROR(NULL, -1, "Invalid wrap type specifier");
		case JAF_HLL_PARAM: _COMPILER_ERROR(NULL, -1, "Invalid hll_param specifier");
		case JAF_HLL_FUNC_71: _COMPILER_ERROR(NULL, -1, "Invalid hll_func_71 specifier");
		case JAF_HLL_FUNC:  _COMPILER_ERROR(NULL, -1, "Invalid hll_func specifier");
		case JAF_IMAIN_SYSTEM: _COMPILER_ERROR(NULL, -1, "Invalid imain_system specifier");
		case JAF_DELEGATE:  return AIN_REF_ARRAY_DELEGATE;
		case JAF_TYPEDEF:   _COMPILER_ERROR(NULL, -1, "Unresolved typedef");
		case JAF_FUNCTYPE:  return AIN_REF_ARRAY_FUNC_TYPE;
		}
	} else if (type->qualifiers & JAF_QUAL_REF) {
		switch (type->type) {
		case JAF_VOID:      _COMPILER_ERROR(NULL, -1, "void ref type");
		case JAF_INT:       return AIN_REF_INT;
		case JAF_BOOL:      return AIN_REF_BOOL;
		case JAF_FLOAT:     return AIN_REF_FLOAT;
		case JAF_LONG_INT:  return AIN_REF_LONG_INT;
		case JAF_STRING:    return AIN_REF_STRING;
		case JAF_STRUCT:    return AIN_REF_STRUCT;
		case JAF_IFACE:     _COMPILER_ERROR(NULL, -1, "Invalid interface type specifier");
		case JAF_ENUM:      return AIN_REF_ENUM;
		case JAF_ARRAY:     _COMPILER_ERROR(NULL, -1, "Invalid array type specifier");
		case JAF_WRAP:      _COMPILER_ERROR(NULL, -1, "Invalid wrap type specifier");
		case JAF_HLL_PARAM: return AIN_REF_HLL_PARAM;
		case JAF_HLL_FUNC_71: _COMPILER_ERROR(NULL, -1, "Invalid hll_func_71 specifier");
		case JAF_HLL_FUNC:  _COMPILER_ERROR(NULL, -1, "Invalid hll_func specifier");
		case JAF_IMAIN_SYSTEM: _COMPILER_ERROR(NULL, -1, "Invalid imain_system specifier");
		case JAF_DELEGATE:  return AIN_REF_DELEGATE;
		case JAF_TYPEDEF:   _COMPILER_ERROR(NULL, -1, "Unresolved typedef");
		case JAF_FUNCTYPE:  return AIN_REF_FUNC_TYPE;
		}
	} else if (type->type == JAF_ARRAY) {
		if (AIN_VERSION_GTE(ain, 11, 0)) {
			return AIN_ARRAY;
		}
		switch (type->array_type->type) {
		case JAF_VOID:      _COMPILER_ERROR(NULL, -1, "void array type");
		case JAF_INT:       return AIN_ARRAY_INT;
		case JAF_BOOL:      return AIN_ARRAY_BOOL;
		case JAF_FLOAT:     return AIN_ARRAY_FLOAT;
		case JAF_LONG_INT:  return AIN_ARRAY_LONG_INT;
		case JAF_STRING:    return AIN_ARRAY_STRING;
		case JAF_STRUCT:    return AIN_ARRAY_STRUCT;
		case JAF_IFACE:     _COMPILER_ERROR(NULL, -1, "Invalid interface type specifier");
		case JAF_ENUM:      _COMPILER_ERROR(NULL, -1, "Enums not supported");
		case JAF_ARRAY:     _COMPILER_ERROR(NULL, -1, "Invalid array type specifier");
		case JAF_WRAP:      _COMPILER_ERROR(NULL, -1, "Invalid wrap type specifier");
		case JAF_HLL_PARAM: _COMPILER_ERROR(NULL, -1, "Invalid hll_param specifier");
		case JAF_HLL_FUNC_71: _COMPILER_ERROR(NULL, -1, "Invalid hll_func_71 specifier");
		case JAF_HLL_FUNC:  _COMPILER_ERROR(NULL, -1, "Invalid hll_func specifier");
		case JAF_IMAIN_SYSTEM: _COMPILER_ERROR(NULL, -1, "Invalid imain_system specifier");
		case JAF_DELEGATE:  return AIN_ARRAY_DELEGATE;
		case JAF_TYPEDEF:   _COMPILER_ERROR(NULL, -1, "Unresolved typedef");
		case JAF_FUNCTYPE:  return AIN_ARRAY_FUNC_TYPE;
		}
	} else {
		return jaf_to_ain_simple_type(type->type);
	}
	_COMPILER_ERROR(NULL, -1, "Unknown type: %d", type);
}

static int jaf_to_ain_struct_type(struct jaf_type_specifier *in)
{
	switch (in->type) {
	case JAF_STRUCT:
	case JAF_IFACE:
	case JAF_FUNCTYPE:
	case JAF_DELEGATE:
	case JAF_ENUM:
		return in->struct_no;
	default:
		return -1;
	}
}

void jaf_to_ain_type(struct ain *ain, struct ain_type *out, struct jaf_type_specifier *in)
{
	out->data = jaf_to_ain_data_type(ain, in);
	out->struc = jaf_to_ain_struct_type(in);
	if (in->type == JAF_ARRAY) {
		out->rank = in->rank;
		if (AIN_VERSION_GTE(ain, 11, 0)) {
			if (in->rank != 1) {
				_JAF_ERROR(NULL, -1, "Only rank-1 arrays supported for ain v11+");
			}
			out->array_type = xcalloc(1, sizeof(struct ain_type));
			jaf_to_ain_type(ain, out->array_type, in->array_type);
			if (out->array_type->data == AIN_STRUCT
					|| out->array_type->data == AIN_REF_STRUCT
					|| out->array_type->data == AIN_IFACE) {
				out->struc = out->array_type->data;
			}
		} else {
			out->struc = jaf_to_ain_struct_type(in->array_type);
			out->array_type = NULL;
		}
	} else if (in->type == JAF_WRAP) {
		if (!AIN_VERSION_GTE(ain, 11, 0)) {
			_JAF_ERROR(NULL, -1, "wrap<> type is ain v11+ only");
		}
		out->rank = 1;
		out->array_type = xcalloc(1, sizeof(struct ain_type));
		jaf_to_ain_type(ain, out->array_type, in->array_type);
	} else {
		out->rank = 0;
		out->array_type = NULL;
	}
}
