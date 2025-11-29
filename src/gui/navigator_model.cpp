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

#include <cstring>
#include "navigator_model.hpp"

extern "C" {
#include "system4/ain.h"
#include "system4/archive.h"
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/flat.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/port.h"
}

NavigatorModel::Node *NavigatorModel::Node::makeKVNode(const char *name, int value)
{
	Node *node = new Node(NavigatorNode::KVNode);
	node->node.kv.name = name;
	node->node.kv.type = NavigatorNode::ValueType::Int;
	node->node.kv.i = value;
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeKVNode(const char *name, float value)
{
	Node *node = new Node(NavigatorNode::KVNode);
	node->node.kv.name = name;
	node->node.kv.type = NavigatorNode::ValueType::Float;
	node->node.kv.f = value;
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeKVNode(const char *name, bool value)
{
	Node *node = new Node(NavigatorNode::KVNode);
	node->node.kv.name = name;
	node->node.kv.type = NavigatorNode::ValueType::Bool;
	node->node.kv.b = value;
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeKVNode(const char *name, struct string *value)
{
	Node *node = new Node(NavigatorNode::KVNode);
	node->node.kv.name = name;
	node->node.kv.type = NavigatorNode::ValueType::String;
	node->node.kv.s = string_ref(value);
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeKVNode_XY(const char *name, float x, float y)
{
	Node *node = new Node(NavigatorNode::KVNode);
	node->node.kv.name = name;
	node->node.kv.type = NavigatorNode::ValueType::Point2;
	node->node.kv.point = { x, y };
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeKVNode_XYZ(const char *name, float x, float y,
		float z)
{
	Node *node = new Node(NavigatorNode::KVNode);
	node->node.kv.name = name;
	node->node.kv.type = NavigatorNode::ValueType::Point3;
	node->node.kv.point = { x, y, z };
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeKVNode_RGB(const char *name, int r, int g,
		int b)
{
	Node *node = new Node(NavigatorNode::KVNode);
	node->node.kv.name = name;
	node->node.kv.type = NavigatorNode::ValueType::Color;
	node->node.kv.color = { r, g, b };
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeKVNode_Rect(const char *name, int x, int y,
		int w, int h)
{
	Node *node = new Node(NavigatorNode::KVNode);
	node->node.kv.name = name;
	node->node.kv.type = NavigatorNode::ValueType::Rect;
	node->node.kv.rect = { x, y, w, h };
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeIndexNode(const char *name, int index)
{
	Node *node = new Node(NavigatorNode::IndexNode);
	node->node.index.name = name;
	node->node.index.i = index;
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::makeCGNode(struct string *name,
		const uint8_t *data, size_t size)
{
	Node *node = new Node(NavigatorNode::CGNode);
	node->node.cg.name = name;
	enum cg_type t = cg_check_format((uint8_t*)data);
	if (t == ALCG_UNKNOWN)
		NOTICE("UNKNOWN CG FORMAT!");
	node->node.cg.data = data;
	node->node.cg.size = size;
	return node;
}

NavigatorModel::Node *NavigatorModel::Node::fromEx(struct ex *ex)
{
        Node *root = new Node(NavigatorNode::RootNode);
        root->appendExFileChildren(ex);
        return root;
}

NavigatorModel::Node *NavigatorModel::Node::fromFlat(struct flat *flat)
{
	Node *root = new Node(NavigatorNode::RootNode);
	root->appendFlatFileChildren(flat);
	return root;
}

NavigatorModel::Node *NavigatorModel::Node::fromExKeyValue(struct string *key, struct ex_value *value)
{
        Node *node = new Node(NavigatorNode::ExStringKeyValueNode);
        node->node.exKV.key.s = key;
        node->node.exKV.value = value;
        node->appendExValueChildren(value);
        return node;
}

NavigatorModel::Node *NavigatorModel::Node::fromExKeyValue(int key, struct ex_value *value)
{
        Node *node = new Node(NavigatorNode::ExIntKeyValueNode);
        node->node.exKV.key.i = key;
        node->node.exKV.value = value;
        node->appendExValueChildren(value);
        return node;
}

NavigatorModel::Node *NavigatorModel::Node::fromExRow(int index, struct ex_table *table, struct ex_field *fields, unsigned nFields)
{
        Node *node = new Node(NavigatorNode::ExRowNode);
        node->node.exRow.i = index;
        node->node.exRow.t = table;

        if (nFields != table->nr_fields) {
                qWarning("Field/Column count mismatch");
                if (table->nr_columns < nFields)
                        nFields = table->nr_columns;
        }

        // add columns for row
        for (unsigned col = 0; col < nFields; col++) {
                node->appendChild(Node::fromExColumn(&table->rows[index][col], &fields[col]));
        }

        return node;
}

NavigatorModel::Node *NavigatorModel::Node::fromExColumn(struct ex_value *value, struct ex_field *field)
{
        Node *node = new Node(NavigatorNode::ExStringKeyValueNode);
        node->node.exKV.key.s = field->name;
        node->node.exKV.value = value;

        if (value->type != EX_TABLE)
                return node;

        // add rows for sub-table
        for (unsigned i = 0; i < value->t->nr_rows; i++) {
                node->appendChild(Node::fromExRow(i, value->t, field->subfields, field->nr_subfields));
        }

        return node;
}

void NavigatorModel::Node::appendExValueChildren(struct ex_value *value)
{
        switch (value->type) {
        case EX_TABLE:
                for (unsigned i = 0; i < value->t->nr_rows; i++) {
                        appendChild(Node::fromExRow(i, value->t, value->t->fields, value->t->nr_fields));
                }
                break;
        case EX_LIST:
                for (unsigned i = 0; i < value->list->nr_items; i++) {
                        appendChild(Node::fromExKeyValue(i, &value->list->items[i].value));
                }
                break;
        case EX_TREE:
                if (value->tree->is_leaf) {
                        node.exKV.key.s = value->tree->leaf.name;
                        node.exKV.value = &value->tree->leaf.value;
                        appendExValueChildren(node.exKV.value);
                } else {
                        for (unsigned i = 0; i < value->tree->nr_children; i++) {
                                appendChild(Node::fromExKeyValue(value->tree->children[i].name, &value->tree->_children[i]));
                        }
                }
                break;
        default:
                break;
        }
}

void NavigatorModel::Node::appendExFileChildren(struct ex *exFile)
{
        for (unsigned i = 0; i < exFile->nr_blocks; i++) {
                appendChild(Node::fromExKeyValue(exFile->blocks[i].name, &exFile->blocks[i].val));
        }
}

static struct string *timeline_typestr(enum flat_timeline_type t)
{
	switch (t) {
	case FLAT_TIMELINE_GRAPHIC: return make_string("Graphic", 7);
	case FLAT_TIMELINE_SOUND:   return make_string("Sound", 5);
	case FLAT_TIMELINE_SCRIPT:  return make_string("Script", 6);
	}
	return make_string("Unknown", 7);
}

void NavigatorModel::Node::appendFlatKeyDataGraphic(struct flat_key_data_graphic *key, int version)
{
	if (version <= 4) {
		appendChild(makeKVNode_XY("Pos", (float)key->pos_x.i, (float)key->pos_y.i));
	} else {
		appendChild(makeKVNode_XY("Pos", key->pos_x.f, key->pos_y.f));
	}
	appendChild(makeKVNode_XY("Scale", key->scale_x, key->scale_y));
	appendChild(makeKVNode_XYZ("Angle", key->angle_x, key->angle_y, key->angle_z));
	appendChild(makeKVNode_RGB("Add", key->add_r, key->add_g, key->add_b));
	appendChild(makeKVNode_RGB("Mul", key->mul_r, key->mul_g, key->mul_b));
	appendChild(makeKVNode("Alpha", (int)key->alpha));
	appendChild(makeKVNode_Rect("Area", key->area_x, key->area_y, key->area_width, key->area_height));
	appendChild(makeKVNode("Draw Filter", (int)key->draw_filter));
	if (version > 8)
		appendChild(makeKVNode("Unknown 1", (int)key->uk1));
	appendChild(makeKVNode_XY("Origin", (float)key->origin_x, (float)key->origin_y));
	if (version > 7)
		appendChild(makeKVNode("Unknown 2", (int)key->uk2));
	appendChild(makeKVNode("Reverse TB", key->reverse_tb));
	appendChild(makeKVNode("Reverse LR", key->reverse_lr));
}

void NavigatorModel::Node::appendFlatKeyFrameGraphic(struct flat_key_frame_graphic *key, int version)
{
	for (unsigned i = 0; i < key->count; i++) {
		Node *node = makeIndexNode("Frame", i);
		node->appendFlatKeyDataGraphic(&key->keys[i], version);
		appendChild(node);
	}
}

void NavigatorModel::Node::appendFlatScriptKey(struct flat_script_key *key, int version)
{
	appendChild(makeKVNode("Frame Index", (int)key->frame_index));
	if (key->has_jump) {
		appendChild(makeKVNode("Jump Frame", (int)key->jump_frame));
	} else if (key->is_stop) {
		appendChild(makeKVNode("Stop", true));
	} else if (key->text) {
		struct string *str = string_conv_output(key->text->text, key->text->size);
		appendChild(makeKVNode("Text", str));
		free_string(str);
	}
}

NavigatorModel::Node *NavigatorModel::Node::fromFlatTimeline(struct flat_timeline *tl, int version)
{
	Node *node = new Node(NavigatorNode::FlatTimelineNode);
	node->node.flat_timeline = tl;

	struct string *libname = string_conv_output(tl->library_name->text, tl->library_name->size);
	struct string *type = timeline_typestr(tl->type);

	node->appendChild(makeKVNode("Library Name", libname));
	node->appendChild(makeKVNode("Type", type));
	node->appendChild(makeKVNode("Begin Frame", (int)tl->begin_frame));
	node->appendChild(makeKVNode("Frame Count", (int)tl->frame_count));

	if (tl->type == FLAT_TIMELINE_GRAPHIC && tl->graphic.count > 0) {
		Node *frames = new Node(NavigatorNode::BranchNode);
		frames->node.name = "Frames";
		for (unsigned i = 0; i < tl->graphic.count; i++) {
			Node *frame = makeIndexNode("Frame", i);
			if (version < 15) {
				frame->appendFlatKeyDataGraphic(&tl->graphic.keys[i], version);
			} else {
				frame->appendFlatKeyFrameGraphic(&tl->graphic.frames[i], version);
			}
			frames->appendChild(frame);
		}
		node->appendChild(frames);
	} else if (tl->type == FLAT_TIMELINE_SCRIPT && tl->script.count > 0) {
		Node *script = new Node(NavigatorNode::BranchNode);
		script->node.name = "Script";
		for (unsigned i = 0; i < tl->script.count; i++) {
			Node *op = makeIndexNode("Op", i);
			op->appendFlatScriptKey(&tl->script.keys[i], version);
			script->appendChild(op);
		}
		node->appendChild(script);
	}

	free_string(type);
	free_string(libname);
	return node;
}

static struct string *flat_library_typestr(enum flat_library_type t)
{
	switch (t) {
	case FLAT_LIB_CG:
		return make_string("CG", 2);
	case FLAT_LIB_MEMORY:
		return make_string("Memory", 6);
	case FLAT_LIB_TIMELINE:
		return make_string("Timeline", 8);
	case FLAT_LIB_STOP_MOTION:
		return make_string("Stop Motion", 11);
	case FLAT_LIB_EMITTER:
		return make_string("Emitter", 7);
	}
	return make_string("Unknown", 7);
}

NavigatorModel::Node *NavigatorModel::Node::fromFlatLibrary(struct flat_library *lib, int version)
{
	Node *node = new Node(NavigatorNode::FlatLibraryNode);
	node->node.flat_library = lib;

	struct string *type = flat_library_typestr(lib->type);
	node->appendChild(makeKVNode("Type", type));
	free_string(type);

	if (lib->type == FLAT_LIB_CG) {
		if (version > 0)
			node->appendChild(makeKVNode("Unknown 1", lib->cg.uk_int));
		struct string *name = string_conv_output(lib->name->text, lib->name->size);
		node->appendChild(makeCGNode(name, lib->cg.data, lib->cg.size));
	} else if (lib->type == FLAT_LIB_TIMELINE) {
		Node *timelines = new Node(NavigatorNode::BranchNode);
		timelines->node.name = "Timelines";
		for (unsigned i = 0; i < lib->timeline.nr_timelines; i++) {
			timelines->appendChild(Node::fromFlatTimeline(&lib->timeline.timelines[i],
						version));
		}
		node->appendChild(timelines);
	} else if (lib->type == FLAT_LIB_STOP_MOTION) {
		struct string *libname = string_conv_output(lib->stop_motion.library_name->text,
				lib->stop_motion.library_name->size);
		node->appendChild(makeKVNode("Library Name", libname));
		node->appendChild(makeKVNode("Span", (int)lib->stop_motion.span));
		node->appendChild(makeKVNode("Loop Type", (int)lib->stop_motion.loop_type));
		free_string(libname);
	} else if (lib->type == FLAT_LIB_EMITTER) {
		struct flat_emitter *em = &lib->emitter;
		struct string *libname = string_conv_output(em->library_name->text,
				em->library_name->size);
		struct string *cgname = string_conv_output(em->end_cg_name->text,
				em->end_cg_name->size);
		node->appendChild(makeKVNode("Library Name", libname));
		node->appendChild(makeKVNode("Unknown Int 1", (int)em->uk_int1));
		node->appendChild(makeKVNode("Create Pos Type", (int)em->create_pos_type));
		node->appendChild(makeKVNode("Create Pos Length", em->create_pos_length));
		node->appendChild(makeKVNode("Create Pos Length 2", em->create_pos_length2));
		node->appendChild(makeKVNode("Create Count", (int)em->create_count));
		node->appendChild(makeKVNode("Particle Length", (int)em->particle_length));
		if (version < 1) {
			node->appendChild(makeKVNode("End Size Rate", em->end_size_rate));
			node->appendChild(makeKVNode_XY("Begin Size Rate", em->begin_x_size_rate,
						em->begin_y_size_rate));
			node->appendChild(makeKVNode_XY("End Size Rate", em->end_x_size_rate,
						em->end_y_size_rate));
		} else {
			node->appendChild(makeKVNode("Begin Size Rate", em->begin_size_rate));
			node->appendChild(makeKVNode("Unknown Size Rate 1", em->uk1_size_rate));
			node->appendChild(makeKVNode("End Size Rate", em->end_size_rate));
			node->appendChild(makeKVNode("Unknown Size Rate 2", em->uk2_size_rate));
			node->appendChild(makeKVNode_XY("Begin Size Rate", em->begin_x_size_rate,
						em->begin_y_size_rate));
			node->appendChild(makeKVNode_XY("End Size Rate", em->end_x_size_rate,
						em->end_y_size_rate));
			node->appendChild(makeKVNode_XY("Unknown Size Rate 1", em->uk1_x_size_rate,
						em->uk1_y_size_rate));
			node->appendChild(makeKVNode_XY("Unknown Size Rate 2", em->uk2_x_size_rate,
						em->uk2_y_size_rate));
			if (version > 5)
				node->appendChild(makeKVNode("Unknown Bool 1", em->uk_bool1));
		}
		node->appendChild(makeKVNode("Direction Type", (int)em->direction_type));
		node->appendChild(makeKVNode_XYZ("Direction", em->direction_x,
					em->direction_y, em->direction_z));
		node->appendChild(makeKVNode("Direction Angle", em->direction_angle));
		node->appendChild(makeKVNode("Is Emitter Connect Type", em->is_emitter_connect_type));
		if (version > 2)
			node->appendChild(makeKVNode("Unknown Int 2", (int)em->uk_int2));
		if (version > 9)
			node->appendChild(makeKVNode("Unknown Int 3", (int)em->uk_int3));
		if (version > 1) {
			node->appendChild(makeKVNode("Unknown Int 4", (int)em->uk_int4));
			node->appendChild(makeKVNode("Unknown Int 5", (int)em->uk_int5));
			node->appendChild(makeKVNode("Unknown Int 6", (int)em->uk_int6));
			node->appendChild(makeKVNode("Unknown Int 7", (int)em->uk_int7));
			node->appendChild(makeKVNode("Unknown Int 8", (int)em->uk_int8));
			node->appendChild(makeKVNode("Unknown Int 9", (int)em->uk_int9));
			node->appendChild(makeKVNode("Unknown Int 10", (int)em->uk_int10));
			node->appendChild(makeKVNode("Unknown Int 11", (int)em->uk_int11));
		}
		node->appendChild(makeKVNode("Speed", em->speed));
		node->appendChild(makeKVNode("Speed Rate", em->speed_rate));
		node->appendChild(makeKVNode("Move Length", em->move_length));
		node->appendChild(makeKVNode("Move Curve", em->mobe_curve));
		if (version > 1)
			node->appendChild(makeKVNode("Unknown Float 1", em->uk_float1));
		node->appendChild(makeKVNode("Is Fall", em->is_fall));
		node->appendChild(makeKVNode("Width", em->width));
		node->appendChild(makeKVNode("Air Resistance", em->air_resistance));
		if (version > 1)
			node->appendChild(makeKVNode("Unknown Bool 2", em->uk_bool2));
		if (version < 1) {

		} else {
			node->appendChild(makeKVNode_XYZ("Begin Angle", em->begin_x_angle,
						em->begin_y_angle, em->begin_z_angle));
			node->appendChild(makeKVNode_XYZ("Unknown Angle 1", em->uk1_x_angle,
						em->uk1_y_angle, em->uk1_z_angle));
			node->appendChild(makeKVNode_XYZ("End Angle", em->end_x_angle,
						em->end_y_angle, em->end_z_angle));
			node->appendChild(makeKVNode_XYZ("Unknown Angle 2", em->uk2_x_angle,
						em->uk2_y_angle, em->uk2_z_angle));
			if (version > 5)
				node->appendChild(makeKVNode("Unknown Bool 3", em->uk_bool3));
		}
		node->appendChild(makeKVNode("Fade-In Frame", (int)em->fade_in_frame));
		node->appendChild(makeKVNode("Fade-Out Frame", (int)em->fade_in_frame));
		node->appendChild(makeKVNode("Draw Filter Type", (int)em->draw_filter_type));
		node->appendChild(makeKVNode("Rand Base", (int)em->rand_base));
		node->appendChild(makeKVNode_XYZ("End Pos", em->end_pos_x, em->end_pos_y,
					em->end_pos_z));
		node->appendChild(makeKVNode("End CG Name", cgname));
		free_string(cgname);
		free_string(libname);
	}
	return node;
}

void NavigatorModel::Node::appendFlatFileChildren(struct flat *flat)
{
	set_input_encoding("CP932");
	set_output_encoding("UTF-8");

	if (flat->hdr.type == FLAT_HDR_V1_32) {
		appendChild(makeKVNode("Header Version", 1));
	} else if (flat->hdr.type == FLAT_HDR_V2_64) {
		appendChild(makeKVNode("Header Version", 2));
		appendChild(makeKVNode("Unknown 1", (int)flat->hdr.uk1));
	} else {
		appendChild(makeKVNode("Header Version", 0));
	}
	appendChild(makeKVNode("FPS", (int)flat->hdr.fps));
	appendChild(makeKVNode("View Width", (int)flat->hdr.game_view_width));
	appendChild(makeKVNode("View Height", (int)flat->hdr.game_view_height));
	appendChild(makeKVNode("Camera Length", flat->hdr.camera_length));
	appendChild(makeKVNode("Meter", flat->hdr.meter));
	appendChild(makeKVNode("Width", (int)flat->hdr.width));
	appendChild(makeKVNode("Height", (int)flat->hdr.height));
	appendChild(makeKVNode("Version", (int)flat->hdr.version));

	if (flat->nr_timelines > 0) {
		Node *timelines = new Node(NavigatorNode::BranchNode);
		timelines->node.name = "Timelines";
		for (unsigned i = 0; i < flat->nr_timelines; i++) {
			timelines->appendChild(Node::fromFlatTimeline(&flat->timelines[i],
						flat->hdr.version));
		}
		appendChild(timelines);
	}

	if (flat->nr_libraries > 0) {
		Node *libraries = new Node(NavigatorNode::BranchNode);
		libraries->node.name = "Libraries";
		for (unsigned i = 0; i < flat->nr_libraries; i++) {
			libraries->appendChild(Node::fromFlatLibrary(&flat->libraries[i],
						flat->hdr.version));
		}
		appendChild(libraries);
	}

	if (flat->nr_talt_entries > 0) {
		Node *talt = new Node(NavigatorNode::BranchNode);
		talt->node.name = "TALT";
		for (unsigned i = 0; i < flat->nr_talt_entries; i++) {
			const uint8_t *data = flat->data + flat->talt_entries[i].off;
			size_t size = flat->talt_entries[i].size;
			struct string *name = make_string("TALT[", 5);
			struct string *n = integer_to_string(i);
			string_append(&name, n);
			string_push_back(&name, ']');
			free_string(n);
			talt->appendChild(makeCGNode(name, data, size));
		}
		appendChild(talt);
	}
}

NavigatorModel::Node *NavigatorModel::Node::fromAin(struct ain *ain)
{
	Node *root = new Node(NavigatorNode::RootNode);

	if (ain->nr_structures > 0) {
		Node *classes = new Node(NavigatorNode::BranchNode);
		classes->node.name = "Classes";
		root->appendChild(classes);

		// add classes
		for (int i = 0; i < ain->nr_structures; i++) {
			classes->appendChild(Node::fromAinItem(ain, i, NavigatorNode::ClassNode));
		}
		// add methods to classes
		for (int i = 0; i < ain->nr_functions; i++) {
			Node *node = classes->child(ain->functions[i].struct_type);
			if (node)
				node->appendChild(Node::fromAinItem(ain, i, NavigatorNode::FunctionNode));
		}
	}

	if (ain->nr_functions > 0) {
		Node *functions = new Node(NavigatorNode::BranchNode);
		functions->node.name = "Functions";
		root->appendChild(functions);

		// add functions
		for (int i = 0; i < ain->nr_functions; i++) {
			functions->appendChild(Node::fromAinItem(ain, i, NavigatorNode::FunctionNode));
		}
	}

	if (ain->nr_enums > 0) {
		Node *enums = new Node(NavigatorNode::BranchNode);
		enums->node.name = "Enumerations";
		root->appendChild(enums);

		// add enums
		for (int i = 0; i < ain->nr_enums; i++) {
			enums->appendChild(Node::fromAinItem(ain, i, NavigatorNode::EnumNode));
		}
		// add methods to enums
		for (int i = 0; i < ain->nr_functions; i++) {
			Node *node = enums->child(ain->functions[i].enum_type);
			if (node)
				node->appendChild(Node::fromAinItem(ain, i, NavigatorNode::FunctionNode));
		}
	}

	if (ain->nr_globals > 0) {
		Node *globals = new Node(NavigatorNode::BranchNode);
		globals->node.name = "Globals";
		root->appendChild(globals);

		// add globals
		for (int i = 0; i < ain->nr_globals; i++) {
			globals->appendChild(Node::fromAinItem(ain, i, NavigatorNode::GlobalNode));
		}
	}

	if (ain->nr_function_types > 0) {
		Node *functypes = new Node(NavigatorNode::BranchNode);
		functypes->node.name = "Function Types";
		root->appendChild(functypes);

		// add function types
		for (int i = 0; i < ain->nr_function_types; i++) {
			functypes->appendChild(Node::fromAinItem(ain, i, NavigatorNode::FuncTypeNode));
		}
	}

	if (ain->nr_delegates > 0) {
		Node *delegates = new Node(NavigatorNode::BranchNode);
		delegates->node.name = "Delegates";
		root->appendChild(delegates);

		// add delegates
		for (int i = 0; i < ain->nr_delegates; i++) {
			delegates->appendChild(Node::fromAinItem(ain, i, NavigatorNode::DelegateNode));
		}
	}

	if (ain->nr_libraries > 0) {
		Node *libraries = new Node(NavigatorNode::BranchNode);
		libraries->node.name = "Libraries";
		root->appendChild(libraries);

		// add libraries
		for (int i = 0; i <  ain->nr_libraries; i++) {
			libraries->appendChild(Node::fromAinItem(ain, i, NavigatorNode::LibraryNode));
		}
	}

	return root;
}

NavigatorModel::Node *NavigatorModel::Node::fromAinItem(struct ain *ain, int i, NavigatorNode::NodeType type)
{
	Node *node = new Node(type);
	node->node.ainItem.ainFile = ain;
	node->node.ainItem.i = i;
	return node;
}

void NavigatorModel::Node::fromArchiveIter(struct archive_data *data, void *user)
{
        Node *parent = static_cast<Node*>(user);
        Node *child = new Node(NavigatorNode::FileNode);
        child->node.ar.type = NavigatorNode::NormalFile;
        child->node.ar.file = archive_copy_descriptor(data);
        child->node.ar.data = NULL;
        parent->appendChild(child);

        const char *ext = file_extension(child->node.ar.file->name);
        if (!ext) {
                // nothing
        } else if (!strcasecmp(ext, "pactex") || !strcasecmp(ext, "ex")) {
                archive_load_file(data);
                set_input_encoding("CP932");
                set_output_encoding("UTF-8");
                struct ex *ex = ex_read_conv(data->data, data->size, string_conv_output);
                if (ex) {
                        child->appendExFileChildren(ex);
                        child->node.ar.type = NavigatorNode::ExFile;
                        child->node.ar.ex = ex;
                } else {
                        // TODO: status message?
                }
                archive_release_file(data);
	} else if (!strcasecmp(ext, "flat")) {
		// XXX: We need to keep a second descriptor with the .flat data
		//      persistently loaded.
		child->node.ar.data = archive_copy_descriptor(child->node.ar.file);
		archive_load_file(child->node.ar.data);
		set_input_encoding("CP932");
		set_output_encoding("UTF-8");
		int error = FLAT_SUCCESS;
		child->node.ar.flat = flat_open(child->node.ar.data->data,
				child->node.ar.data->size, &error);
		if (child->node.ar.flat) {
			child->appendFlatFileChildren(child->node.ar.flat);
			child->node.ar.type = NavigatorNode::FlatFile;
		} else {
			// TODO: status message?
		}
	}
}

NavigatorModel::Node *NavigatorModel::Node::fromArchive(struct archive *ar)
{
        Node *root = new Node(NavigatorNode::RootNode);
        root->appendArchiveChildren(ar);
        return root;
}

void NavigatorModel::Node::appendArchiveChildren(struct archive *ar)
{
        archive_for_each(ar, fromArchiveIter, this);
}

NavigatorModel::Node::Node(NavigatorNode::NodeType type)
	: parentNode(nullptr)
{
	node.type = type;
}

NavigatorModel::Node::~Node()
{
        qDeleteAll(children);

        if (node.type == NavigatorNode::FileNode) {
                archive_free_data(node.ar.file);
                switch (node.ar.type) {
                case NavigatorNode::NormalFile:
                        break;
                case NavigatorNode::ExFile:
                        ex_free(node.ar.ex);
                        break;
		case NavigatorNode::FlatFile:
			flat_free(node.ar.flat);
			break;
                case NavigatorNode::ArFile:
                        archive_free(node.ar.ar);
                        break;
                }
		if (node.ar.data)
			archive_free_data(node.ar.data);
        } else if (node.type == NavigatorNode::KVNode) {
		if (node.kv.type == NavigatorNode::String) {
			free_string(node.kv.s);
		}
	} else if (node.type == NavigatorNode::CGNode) {
		free_string(node.cg.name);
	}
}

NavigatorModel::Node *NavigatorModel::Node::child(int i)
{
        if (i < 0 || i >= children.size())
                return nullptr;
        return children[i];
}

NavigatorModel::Node *NavigatorModel::Node::parent()
{
        return parentNode;
}

void NavigatorModel::Node::appendChild(NavigatorModel::Node *child)
{
        children.append(child);
        child->parentNode = this;
}

int NavigatorModel::Node::row()
{
        if (parentNode)
                return parentNode->children.indexOf(const_cast<Node*>(this));
        return 0;
}

int NavigatorModel::Node::childCount()
{
        return children.size();
}

int NavigatorModel::Node::columnCount()
{
        return 3;
}

QVariant NavigatorModel::Node::data(int column) const
{
	if (column == 0) {
		return node.getName();
	} else if (column == 1) {
		return node.getType();
	} else if (column == 2) {
		return node.getValue();
	}
	return QVariant();
}

NavigatorModel *NavigatorModel::fromExFile(std::shared_ptr<struct ex> ex, QObject *parent)
{
	NavigatorModel *model = new NavigatorModel(parent);
	model->root = Node::fromEx(ex.get());
	model->exFile = ex;
	return model;
}

NavigatorModel *NavigatorModel::fromFlatFile(std::shared_ptr<struct flat> flat, QObject *parent)
{
	NavigatorModel *model = new NavigatorModel(parent);
	model->root = Node::fromFlat(flat.get());
	model->flatFile = flat;
	return model;
}

NavigatorModel *NavigatorModel::fromAinFile(std::shared_ptr<struct ain> ain, QObject *parent)
{
	NavigatorModel *model = new NavigatorModel(parent);
	model->root = Node::fromAin(ain.get());
	model->ainFile = ain;
	return model;
}

NavigatorModel *NavigatorModel::fromArchive(std::shared_ptr<struct archive> ar, QObject *parent)
{
	NavigatorModel *model = new NavigatorModel(parent);
	model->root = Node::fromArchive(ar.get());
	model->arFile = ar;
	return model;
}

NavigatorModel::~NavigatorModel()
{
        delete root;
}

QModelIndex NavigatorModel::index(int row, int column, const QModelIndex &parent) const
{
        if (!hasIndex(row, column, parent))
                return QModelIndex();

        Node *parentNode;
        if (!parent.isValid())
                parentNode = root;
        else
                parentNode = static_cast<Node*>(parent.internalPointer());

        Node *childNode = parentNode->child(row);
        if (childNode)
                return createIndex(row, column, childNode);
        return QModelIndex();
}

QModelIndex NavigatorModel::parent(const QModelIndex &index) const
{
        if (!index.isValid())
                return QModelIndex();

        Node *childNode = static_cast<Node*>(index.internalPointer());
        Node *parentNode = childNode->parent();

        if (!parentNode || parentNode == root)
                return QModelIndex();

        return createIndex(parentNode->row(), 0, parentNode);
}

int NavigatorModel::rowCount(const QModelIndex &parent) const
{
        if (parent.column() > 0)
                return 0;

        Node *parentNode;
        if (!parent.isValid())
                parentNode = root;
        else
                parentNode = static_cast<Node*>(parent.internalPointer());

        return parentNode->childCount();
}

int NavigatorModel::columnCount(const QModelIndex &parent) const
{
        if (parent.isValid())
                return static_cast<Node*>(parent.internalPointer())->columnCount();
        return root->columnCount();
}

QVariant NavigatorModel::data(const QModelIndex &index, int role) const
{
        if (!index.isValid())
                return QVariant();

        if (role != Qt::DisplayRole)
                return QVariant();

        Node *node = static_cast<Node*>(index.internalPointer());
        return node->data(index.column());
}

Qt::ItemFlags NavigatorModel::flags(const QModelIndex &index) const
{
        if (!index.isValid())
                return Qt::NoItemFlags;
        return QAbstractItemModel::flags(index);
}

QVariant NavigatorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
                return root->data(section);
        return QVariant();
}

NavigatorNode *NavigatorModel::getNode(const QModelIndex &index) const
{
	if (!index.isValid())
		return nullptr;
	return &static_cast<Node*>(index.internalPointer())->node;
}
