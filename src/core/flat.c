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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "cJSON.h"
#include "system4.h"
#include "system4/buffer.h"
#include "system4/cg.h"
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/flat.h"
#include "system4/little_endian.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ex.h"
#include "alice/flat.h"
#include "alice/json.h"

static cJSON *string_to_json(const struct string *s)
{
	if (!s)
		return cJSON_CreateNull();
	struct string *u = string_conv_output(s->text, s->size);
	cJSON *j = cJSON_CreateString(u->text);
	free_string(u);
	return j;
}

static struct string *string_from_json(const char *s)
{
	if (!s)
		return NULL;
	return string_conv_output(s, strlen(s));
}

// ---- FLAT header JSON ------------------------------------------------------

static cJSON *flat_header_to_json(const struct flat_header *hdr)
{
	cJSON *o = cJSON_CreateObject();
	switch (hdr->type) {
	case FLAT_HDR_V1_32:
		cJSON_AddStringToObject(o, "type", "v1_32");
		break;
	case FLAT_HDR_V2_64:
		cJSON_AddStringToObject(o, "type", "v2_64");
		break;
	default:
		ALICE_ERROR("flat header: unsupported type %d", hdr->type);
	}
	cJSON_AddNumberToObject(o, "version", hdr->version);
	cJSON_AddNumberToObject(o, "fps", hdr->fps);
	cJSON_AddNumberToObject(o, "game_view_width", hdr->game_view_width);
	cJSON_AddNumberToObject(o, "game_view_height", hdr->game_view_height);
	cJSON_AddNumberToObject(o, "camera_length", hdr->camera_length);
	cJSON_AddNumberToObject(o, "meter", hdr->meter);
	cJSON_AddNumberToObject(o, "width", hdr->width);
	cJSON_AddNumberToObject(o, "height", hdr->height);
	if (hdr->type == FLAT_HDR_V2_64)
		cJSON_AddNumberToObject(o, "uk1", hdr->uk1);
	return o;
}

static void flat_header_from_json(cJSON *j, struct flat_header *out)
{
	memset(out, 0, sizeof(*out));
	out->present = true;
	const char *type = json_get_string(j, "type");
	if (!strcmp(type, "v1_32"))
		out->type = FLAT_HDR_V1_32;
	else if (!strcmp(type, "v2_64"))
		out->type = FLAT_HDR_V2_64;
	else
		ALICE_ERROR("flat header JSON: unknown type '%s'", type);
	out->version = json_get_int(j, "version");
	out->fps = json_get_int(j, "fps");
	out->game_view_width = json_get_int(j, "game_view_width");
	out->game_view_height = json_get_int(j, "game_view_height");
	out->camera_length = json_get_double(j, "camera_length");
	out->meter = json_get_double(j, "meter");
	out->width = json_get_int(j, "width");
	out->height = json_get_int(j, "height");
	if (out->type == FLAT_HDR_V2_64)
		out->uk1 = json_get_int_or(j, "uk1", 0);
}

// ---- graphic key data ------------------------------------------------------

// Keyframes are stored delta-compressed: a property is omitted from the JSON
// when it is equal to the corresponding property of the previous key. On the
// build side, a missing property inherits its value from the previous key (or
// the base default if prev == NULL).

static cJSON *graphic_key_to_json(const struct flat_key_data_graphic *k, int version,
                                  const struct flat_key_data_graphic *prev)
{
	cJSON *o = cJSON_CreateObject();
	if (!prev || k->pos_x != prev->pos_x) cJSON_AddNumberToObject(o, "pos_x", k->pos_x);
	if (!prev || k->pos_y != prev->pos_y) cJSON_AddNumberToObject(o, "pos_y", k->pos_y);
	if (!prev || k->scale_x != prev->scale_x) cJSON_AddNumberToObject(o, "scale_x", k->scale_x);
	if (!prev || k->scale_y != prev->scale_y) cJSON_AddNumberToObject(o, "scale_y", k->scale_y);
	if (!prev || k->angle_x != prev->angle_x) cJSON_AddNumberToObject(o, "angle_x", k->angle_x);
	if (!prev || k->angle_y != prev->angle_y) cJSON_AddNumberToObject(o, "angle_y", k->angle_y);
	if (!prev || k->angle_z != prev->angle_z) cJSON_AddNumberToObject(o, "angle_z", k->angle_z);
	if (!prev || k->add_r != prev->add_r) cJSON_AddNumberToObject(o, "add_r", k->add_r);
	if (!prev || k->add_g != prev->add_g) cJSON_AddNumberToObject(o, "add_g", k->add_g);
	if (!prev || k->add_b != prev->add_b) cJSON_AddNumberToObject(o, "add_b", k->add_b);
	if (!prev || k->mul_r != prev->mul_r) cJSON_AddNumberToObject(o, "mul_r", k->mul_r);
	if (!prev || k->mul_g != prev->mul_g) cJSON_AddNumberToObject(o, "mul_g", k->mul_g);
	if (!prev || k->mul_b != prev->mul_b) cJSON_AddNumberToObject(o, "mul_b", k->mul_b);
	if (!prev || k->alpha != prev->alpha) cJSON_AddNumberToObject(o, "alpha", k->alpha);
	if (!prev || k->area_x != prev->area_x) cJSON_AddNumberToObject(o, "area_x", k->area_x);
	if (!prev || k->area_y != prev->area_y) cJSON_AddNumberToObject(o, "area_y", k->area_y);
	if (!prev || k->area_width != prev->area_width) cJSON_AddNumberToObject(o, "area_width", k->area_width);
	if (!prev || k->area_height != prev->area_height) cJSON_AddNumberToObject(o, "area_height", k->area_height);
	if (!prev || k->draw_filter != prev->draw_filter) cJSON_AddNumberToObject(o, "draw_filter", k->draw_filter);
	if (version > 8 && (!prev || k->uk1 != prev->uk1)) cJSON_AddNumberToObject(o, "uk1", k->uk1);
	if (!prev || k->origin_x != prev->origin_x) cJSON_AddNumberToObject(o, "origin_x", k->origin_x);
	if (!prev || k->origin_y != prev->origin_y) cJSON_AddNumberToObject(o, "origin_y", k->origin_y);
	if (version > 7 && (!prev || k->uk2 != prev->uk2)) cJSON_AddNumberToObject(o, "uk2", k->uk2);
	if (!prev || k->reverse_tb != prev->reverse_tb) cJSON_AddBoolToObject(o, "reverse_tb", k->reverse_tb);
	if (!prev || k->reverse_lr != prev->reverse_lr) cJSON_AddBoolToObject(o, "reverse_lr", k->reverse_lr);
	return o;
}

static void graphic_key_from_json(cJSON *j, struct flat_key_data_graphic *k, int version,
                                  const struct flat_key_data_graphic *prev)
{
	memset(k, 0, sizeof(*k));
	k->pos_x = json_get_double_or(j, "pos_x", prev ? prev->pos_x : 0);
	k->pos_y = json_get_double_or(j, "pos_y", prev ? prev->pos_y : 0);
	k->scale_x = json_get_double_or(j, "scale_x", prev ? prev->scale_x : 1);
	k->scale_y = json_get_double_or(j, "scale_y", prev ? prev->scale_y : 1);
	k->angle_x = json_get_double_or(j, "angle_x", prev ? prev->angle_x : 0);
	k->angle_y = json_get_double_or(j, "angle_y", prev ? prev->angle_y : 0);
	k->angle_z = json_get_double_or(j, "angle_z", prev ? prev->angle_z : 0);
	k->add_r = json_get_int_or(j, "add_r", prev ? prev->add_r : 0);
	k->add_g = json_get_int_or(j, "add_g", prev ? prev->add_g : 0);
	k->add_b = json_get_int_or(j, "add_b", prev ? prev->add_b : 0);
	k->mul_r = json_get_int_or(j, "mul_r", prev ? prev->mul_r : 0);
	k->mul_g = json_get_int_or(j, "mul_g", prev ? prev->mul_g : 0);
	k->mul_b = json_get_int_or(j, "mul_b", prev ? prev->mul_b : 0);
	k->alpha = json_get_int_or(j, "alpha", prev ? prev->alpha : 255);
	k->area_x = json_get_int_or(j, "area_x", prev ? prev->area_x : 0);
	k->area_y = json_get_int_or(j, "area_y", prev ? prev->area_y : 0);
	k->area_width = json_get_int_or(j, "area_width", prev ? prev->area_width : 0);
	k->area_height = json_get_int_or(j, "area_height", prev ? prev->area_height : 0);
	k->draw_filter = json_get_int_or(j, "draw_filter", prev ? prev->draw_filter : 0);
	if (version > 8)
		k->uk1 = json_get_int_or(j, "uk1", prev ? prev->uk1 : 0);
	k->origin_x = json_get_int_or(j, "origin_x", prev ? prev->origin_x : 0);
	k->origin_y = json_get_int_or(j, "origin_y", prev ? prev->origin_y : 0);
	if (version > 7)
		k->uk2 = json_get_int_or(j, "uk2", prev ? prev->uk2 : 0);
	k->reverse_tb = json_get_bool_or(j, "reverse_tb", prev ? prev->reverse_tb : false);
	k->reverse_lr = json_get_bool_or(j, "reverse_lr", prev ? prev->reverse_lr : false);
}

// ---- timeline --------------------------------------------------------------

static cJSON *timeline_to_json(const struct flat_timeline *tl, int version)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddItemToObject(o, "name", string_to_json(tl->name));
	cJSON_AddItemToObject(o, "library_name", string_to_json(tl->library_name));
	cJSON_AddNumberToObject(o, "begin_frame", tl->begin_frame);
	cJSON_AddNumberToObject(o, "frame_count", tl->frame_count);

	switch (tl->type) {
	case FLAT_TIMELINE_GRAPHIC: {
		cJSON_AddStringToObject(o, "type", "graphic");
		if (version < 15) {
			cJSON *a = cJSON_CreateArray();
			for (uint32_t i = 0; i < tl->graphic.count; i++) {
				const struct flat_key_data_graphic *prev = i > 0 ? &tl->graphic.keys[i-1] : NULL;
				cJSON_AddItemToArray(a, graphic_key_to_json(&tl->graphic.keys[i], version, prev));
			}
			cJSON_AddItemToObject(o, "keys", a);
		} else {
			cJSON *frames = cJSON_CreateArray();
			for (int32_t f = 0; f < tl->frame_count; f++) {
				cJSON *fo = cJSON_CreateObject();
				cJSON *keys = cJSON_CreateArray();
				// Delta against the same key index in the previous frame.
				for (uint32_t i = 0; i < tl->graphic.frames[f].count; i++) {
					const struct flat_key_data_graphic *prev = NULL;
					if (f > 0 && i < tl->graphic.frames[f-1].count)
						prev = &tl->graphic.frames[f-1].keys[i];
					cJSON_AddItemToArray(keys, graphic_key_to_json(&tl->graphic.frames[f].keys[i], version, prev));
				}
				cJSON_AddItemToObject(fo, "keys", keys);
				cJSON_AddItemToArray(frames, fo);
			}
			cJSON_AddItemToObject(o, "frames", frames);
		}
		break;
	}
	case FLAT_TIMELINE_SCRIPT: {
		cJSON_AddStringToObject(o, "type", "script");
		cJSON *a = cJSON_CreateArray();
		for (uint32_t i = 0; i < tl->script.count; i++) {
			const struct flat_script_key *k = &tl->script.keys[i];
			cJSON *ko = cJSON_CreateObject();
			cJSON_AddNumberToObject(ko, "frame_index", k->frame_index);
			if (k->has_jump)
				cJSON_AddNumberToObject(ko, "jump_frame", k->jump_frame);
			if (k->is_stop)
				cJSON_AddBoolToObject(ko, "is_stop", true);
			if (k->text)
				cJSON_AddItemToObject(ko, "text", string_to_json(k->text));
			cJSON_AddItemToArray(a, ko);
		}
		cJSON_AddItemToObject(o, "keys", a);
		break;
	}
	case FLAT_TIMELINE_SOUND:
		cJSON_AddStringToObject(o, "type", "sound");
		ALICE_ERROR("FLAT_TIMELINE_SOUND not implemented");
	default:
		ALICE_ERROR("Unknown timeline type %d", tl->type);
	}
	return o;
}

static void timeline_from_json(cJSON *j, struct flat_timeline *tl, int version)
{
	memset(tl, 0, sizeof(*tl));
	tl->name = string_from_json(json_get_string(j, "name"));
	tl->library_name = string_from_json(json_get_string(j, "library_name"));
	tl->begin_frame = json_get_int(j, "begin_frame");
	tl->frame_count = json_get_int(j, "frame_count");

	const char *type = json_get_string(j, "type");
	if (!strcmp(type, "graphic")) {
		tl->type = FLAT_TIMELINE_GRAPHIC;
		if (version < 15) {
			cJSON *keys = json_get_array(j, "keys");
			int n = cJSON_GetArraySize(keys);
			tl->graphic.count = n;
			tl->graphic.keys = xcalloc(n, sizeof(*tl->graphic.keys));
			for (int i = 0; i < n; i++) {
				const struct flat_key_data_graphic *prev = i > 0 ? &tl->graphic.keys[i-1] : NULL;
				graphic_key_from_json(cJSON_GetArrayItem(keys, i),
				                      &tl->graphic.keys[i], version, prev);
			}
		} else {
			cJSON *frames = json_get_array(j, "frames");
			int nframes = cJSON_GetArraySize(frames);
			if (nframes != tl->frame_count)
				ALICE_ERROR("timeline: frames array length %d != frame_count %d",
				            nframes, tl->frame_count);
			tl->graphic.frames = xcalloc(nframes, sizeof(*tl->graphic.frames));
			for (int f = 0; f < nframes; f++) {
				cJSON *fo = cJSON_GetArrayItem(frames, f);
				cJSON *keys = json_get_array(fo, "keys");
				int nk = cJSON_GetArraySize(keys);
				tl->graphic.frames[f].count = nk;
				tl->graphic.frames[f].keys = xcalloc(nk, sizeof(struct flat_key_data_graphic));
				for (int i = 0; i < nk; i++) {
					const struct flat_key_data_graphic *prev = NULL;
					if (f > 0 && (uint32_t)i < tl->graphic.frames[f-1].count)
						prev = &tl->graphic.frames[f-1].keys[i];
					graphic_key_from_json(cJSON_GetArrayItem(keys, i),
					                      &tl->graphic.frames[f].keys[i], version, prev);
				}
			}
		}
	} else if (!strcmp(type, "script")) {
		tl->type = FLAT_TIMELINE_SCRIPT;
		cJSON *keys = json_get_array(j, "keys");
		int n = cJSON_GetArraySize(keys);
		tl->script.count = n;
		tl->script.keys = xcalloc(n, sizeof(*tl->script.keys));
		for (int i = 0; i < n; i++) {
			cJSON *ko = cJSON_GetArrayItem(keys, i);
			struct flat_script_key *k = &tl->script.keys[i];
			k->frame_index = json_get_int(ko, "frame_index");
			cJSON *jj = cJSON_GetObjectItem(ko, "jump_frame");
			if (jj) {
				k->has_jump = true;
				k->jump_frame = jj->valueint;
			}
			k->is_stop = json_get_bool_or(ko, "is_stop", false);
			const char *text = json_get_string_or_null(ko, "text");
			if (text)
				k->text = string_from_json(text);
		}
	} else {
		ALICE_ERROR("Unknown timeline type '%s'", type);
	}
}

// ---- MTLC ------------------------------------------------------------------

static cJSON *flat_mtlc_to_json(const struct flat_timeline *tls, size_t n, int version)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *a = cJSON_CreateArray();
	for (size_t i = 0; i < n; i++)
		cJSON_AddItemToArray(a, timeline_to_json(&tls[i], version));
	cJSON_AddItemToObject(root, "timelines", a);
	return root;
}

static void flat_mtlc_from_json(cJSON *j, struct flat_timeline **out, size_t *nr_out, int version)
{
	cJSON *a = json_get_array(j, "timelines");
	int n = cJSON_GetArraySize(a);
	struct flat_timeline *tls = xcalloc(n, sizeof(*tls));
	for (int i = 0; i < n; i++)
		timeline_from_json(cJSON_GetArrayItem(a, i), &tls[i], version);
	*out = tls;
	*nr_out = n;
}

// ---- stop_motion -----------------------------------------------------------

static cJSON *stop_motion_to_json(const struct flat_stop_motion *sm)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "kind", "stop_motion");
	cJSON_AddItemToObject(o, "library_name", string_to_json(sm->library_name));
	cJSON_AddNumberToObject(o, "span", sm->span);
	cJSON_AddNumberToObject(o, "loop_type", sm->loop_type);
	return o;
}

static void stop_motion_from_json(cJSON *j, struct flat_stop_motion *sm)
{
	memset(sm, 0, sizeof(*sm));
	sm->library_name = string_from_json(json_get_string(j, "library_name"));
	sm->span = json_get_int(j, "span");
	sm->loop_type = json_get_int(j, "loop_type");
}

// ---- emitter ---------------------------------------------------------------

static cJSON *emitter_to_json(const struct flat_emitter *em)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "kind", "emitter");
	cJSON_AddItemToObject(o, "library_name", string_to_json(em->library_name));
	cJSON_AddNumberToObject(o, "particle_align", em->particle_align);
	cJSON_AddNumberToObject(o, "create_pos_type", em->create_pos_type);
	cJSON_AddNumberToObject(o, "create_pos_length", em->create_pos_length);
	cJSON_AddNumberToObject(o, "create_pos_length2", em->create_pos_length2);
	cJSON_AddNumberToObject(o, "create_count", em->create_count);
	cJSON_AddNumberToObject(o, "particle_lifetime", em->particle_lifetime);
	cJSON_AddNumberToObject(o, "begin_scale", em->begin_scale);
	cJSON_AddNumberToObject(o, "begin_scale_rand", em->begin_scale_rand);
	cJSON_AddNumberToObject(o, "end_scale", em->end_scale);
	cJSON_AddNumberToObject(o, "end_scale_rand", em->end_scale_rand);
	cJSON_AddNumberToObject(o, "begin_x_scale", em->begin_x_scale);
	cJSON_AddNumberToObject(o, "begin_x_scale_rand", em->begin_x_scale_rand);
	cJSON_AddNumberToObject(o, "end_x_scale", em->end_x_scale);
	cJSON_AddNumberToObject(o, "end_x_scale_rand", em->end_x_scale_rand);
	cJSON_AddNumberToObject(o, "begin_y_scale", em->begin_y_scale);
	cJSON_AddNumberToObject(o, "begin_y_scale_rand", em->begin_y_scale_rand);
	cJSON_AddNumberToObject(o, "end_y_scale", em->end_y_scale);
	cJSON_AddNumberToObject(o, "end_y_scale_rand", em->end_y_scale_rand);
	cJSON_AddBoolToObject(o, "sync_scale_rand", em->sync_scale_rand);
	cJSON_AddNumberToObject(o, "direction_type", em->direction_type);
	cJSON_AddNumberToObject(o, "direction_x", em->direction_x);
	cJSON_AddNumberToObject(o, "direction_y", em->direction_y);
	cJSON_AddNumberToObject(o, "direction_z", em->direction_z);
	cJSON_AddNumberToObject(o, "direction_angle", em->direction_angle);
	cJSON_AddNumberToObject(o, "parent_key_mode", em->parent_key_mode);
	cJSON_AddNumberToObject(o, "pos_track_mode", em->pos_track_mode);
	cJSON_AddNumberToObject(o, "uk_int3", em->uk_int3);
	cJSON_AddNumberToObject(o, "inherit_alpha", em->inherit_alpha);
	cJSON_AddNumberToObject(o, "inherit_rotation", em->inherit_rotation);
	cJSON_AddNumberToObject(o, "inherit_scale", em->inherit_scale);
	cJSON_AddNumberToObject(o, "inherit_add_color", em->inherit_add_color);
	cJSON_AddNumberToObject(o, "inherit_mul_color", em->inherit_mul_color);
	cJSON_AddNumberToObject(o, "inherit_draw_filter", em->inherit_draw_filter);
	cJSON_AddNumberToObject(o, "inherit_reverse_lr", em->inherit_reverse_lr);
	cJSON_AddNumberToObject(o, "inherit_reverse_tb", em->inherit_reverse_tb);
	cJSON_AddNumberToObject(o, "speed", em->speed);
	cJSON_AddNumberToObject(o, "acceleration", em->acceleration);
	cJSON_AddNumberToObject(o, "move_length", em->move_length);
	cJSON_AddNumberToObject(o, "move_curve", em->move_curve);
	cJSON_AddNumberToObject(o, "move_rand", em->move_rand);
	cJSON_AddBoolToObject(o, "is_fall", em->is_fall);
	cJSON_AddNumberToObject(o, "width", em->width);
	cJSON_AddNumberToObject(o, "air_resistance", em->air_resistance);
	cJSON_AddBoolToObject(o, "align_to_direction", em->align_to_direction);
	cJSON_AddNumberToObject(o, "begin_x_angle", em->begin_x_angle);
	cJSON_AddNumberToObject(o, "begin_x_angle_rand", em->begin_x_angle_rand);
	cJSON_AddNumberToObject(o, "end_x_angle", em->end_x_angle);
	cJSON_AddNumberToObject(o, "end_x_angle_rand", em->end_x_angle_rand);
	cJSON_AddNumberToObject(o, "begin_y_angle", em->begin_y_angle);
	cJSON_AddNumberToObject(o, "begin_y_angle_rand", em->begin_y_angle_rand);
	cJSON_AddNumberToObject(o, "end_y_angle", em->end_y_angle);
	cJSON_AddNumberToObject(o, "end_y_angle_rand", em->end_y_angle_rand);
	cJSON_AddNumberToObject(o, "begin_z_angle", em->begin_z_angle);
	cJSON_AddNumberToObject(o, "begin_z_angle_rand", em->begin_z_angle_rand);
	cJSON_AddNumberToObject(o, "end_z_angle", em->end_z_angle);
	cJSON_AddNumberToObject(o, "end_z_angle_rand", em->end_z_angle_rand);
	cJSON_AddBoolToObject(o, "sync_rotation_rand", em->sync_rotation_rand);
	cJSON_AddNumberToObject(o, "fade_in_frame", em->fade_in_frame);
	cJSON_AddNumberToObject(o, "fade_out_frame", em->fade_out_frame);
	cJSON_AddNumberToObject(o, "draw_filter", em->draw_filter);
	cJSON_AddNumberToObject(o, "rand_seed", em->rand_seed);
	cJSON_AddNumberToObject(o, "end_pos_type", em->end_pos_type);
	cJSON_AddNumberToObject(o, "end_pos_x", em->end_pos_x);
	cJSON_AddNumberToObject(o, "end_pos_y", em->end_pos_y);
	cJSON_AddNumberToObject(o, "end_pos_z", em->end_pos_z);
	cJSON_AddItemToObject(o, "end_cg_name", string_to_json(em->end_cg_name));
	return o;
}

static void emitter_from_json(cJSON *j, struct flat_emitter *em)
{
	memset(em, 0, sizeof(*em));
	em->library_name = string_from_json(json_get_string(j, "library_name"));
	em->particle_align = json_get_int_or(j, "particle_align", 5);
	em->create_pos_type = json_get_int(j, "create_pos_type");
	em->create_pos_length = json_get_double(j, "create_pos_length");
	em->create_pos_length2 = json_get_double(j, "create_pos_length2");
	em->create_count = json_get_int(j, "create_count");
	em->particle_lifetime = json_get_int(j, "particle_lifetime");
	em->begin_scale = json_get_double(j, "begin_scale");
	em->begin_scale_rand = json_get_double_or(j, "begin_scale_rand", 0);
	em->end_scale = json_get_double(j, "end_scale");
	em->end_scale_rand = json_get_double_or(j, "end_scale_rand", 0);
	em->begin_x_scale = json_get_double(j, "begin_x_scale");
	em->begin_x_scale_rand = json_get_double_or(j, "begin_x_scale_rand", 0);
	em->end_x_scale = json_get_double(j, "end_x_scale");
	em->end_x_scale_rand = json_get_double_or(j, "end_x_scale_rand", 0);
	em->begin_y_scale = json_get_double(j, "begin_y_scale");
	em->begin_y_scale_rand = json_get_double_or(j, "begin_y_scale_rand", 0);
	em->end_y_scale = json_get_double(j, "end_y_scale");
	em->end_y_scale_rand = json_get_double_or(j, "end_y_scale_rand", 0);
	em->sync_scale_rand = json_get_bool_or(j, "sync_scale_rand", false);
	em->direction_type = json_get_int(j, "direction_type");
	em->direction_x = json_get_double(j, "direction_x");
	em->direction_y = json_get_double(j, "direction_y");
	em->direction_z = json_get_double(j, "direction_z");
	em->direction_angle = json_get_double(j, "direction_angle");
	em->parent_key_mode = json_get_int(j, "parent_key_mode");
	em->pos_track_mode = json_get_int_or(j, "pos_track_mode", 2);
	em->uk_int3 = json_get_int_or(j, "uk_int3", 0);
	em->inherit_alpha = json_get_int_or(j, "inherit_alpha", 0);
	em->inherit_rotation = json_get_int_or(j, "inherit_rotation", 0);
	em->inherit_scale = json_get_int_or(j, "inherit_scale", 0);
	em->inherit_add_color = json_get_int_or(j, "inherit_add_color", 0);
	em->inherit_mul_color = json_get_int_or(j, "inherit_mul_color", 0);
	em->inherit_draw_filter = json_get_int_or(j, "inherit_draw_filter", 0);
	em->inherit_reverse_lr = json_get_int_or(j, "inherit_reverse_lr", 0);
	em->inherit_reverse_tb = json_get_int_or(j, "inherit_reverse_tb", 0);
	em->speed = json_get_double(j, "speed");
	em->acceleration = json_get_double(j, "acceleration");
	em->move_length = json_get_double(j, "move_length");
	em->move_curve = json_get_double(j, "move_curve");
	em->move_rand = json_get_double_or(j, "move_rand", 0);
	em->is_fall = json_get_bool(j, "is_fall");
	em->width = json_get_double(j, "width");
	em->air_resistance = json_get_double(j, "air_resistance");
	em->align_to_direction = json_get_bool_or(j, "align_to_direction", false);
	em->begin_x_angle = json_get_double(j, "begin_x_angle");
	em->begin_x_angle_rand = json_get_double_or(j, "begin_x_angle_rand", 0);
	em->end_x_angle = json_get_double(j, "end_x_angle");
	em->end_x_angle_rand = json_get_double_or(j, "end_x_angle_rand", 0);
	em->begin_y_angle = json_get_double(j, "begin_y_angle");
	em->begin_y_angle_rand = json_get_double_or(j, "begin_y_angle_rand", 0);
	em->end_y_angle = json_get_double(j, "end_y_angle");
	em->end_y_angle_rand = json_get_double_or(j, "end_y_angle_rand", 0);
	em->begin_z_angle = json_get_double(j, "begin_z_angle");
	em->begin_z_angle_rand = json_get_double_or(j, "begin_z_angle_rand", 0);
	em->end_z_angle = json_get_double(j, "end_z_angle");
	em->end_z_angle_rand = json_get_double_or(j, "end_z_angle_rand", 0);
	em->sync_rotation_rand = json_get_bool_or(j, "sync_rotation_rand", false);
	em->fade_in_frame = json_get_int(j, "fade_in_frame");
	em->fade_out_frame = json_get_int(j, "fade_out_frame");
	em->draw_filter = json_get_int(j, "draw_filter");
	em->rand_seed = json_get_int(j, "rand_seed");
	em->end_pos_type = json_get_int(j, "end_pos_type");
	em->end_pos_x = json_get_double(j, "end_pos_x");
	em->end_pos_y = json_get_double(j, "end_pos_y");
	em->end_pos_z = json_get_double(j, "end_pos_z");
	em->end_cg_name = string_from_json(json_get_string(j, "end_cg_name"));
}

// ---- LIBL JSON dispatch ----------------------------------------------------

static cJSON *flat_library_to_json(const struct flat_library *lib, int version)
{
	switch (lib->type) {
	case FLAT_LIB_TIMELINE: {
		cJSON *o = cJSON_CreateObject();
		cJSON_AddStringToObject(o, "kind", "timeline");
		cJSON *a = cJSON_CreateArray();
		for (size_t i = 0; i < lib->timeline.nr_timelines; i++)
			cJSON_AddItemToArray(a, timeline_to_json(&lib->timeline.timelines[i], version));
		cJSON_AddItemToObject(o, "timelines", a);
		return o;
	}
	case FLAT_LIB_STOP_MOTION:
		return stop_motion_to_json(&lib->stop_motion);
	case FLAT_LIB_EMITTER:
		return emitter_to_json(&lib->emitter);
	default:
		ALICE_ERROR("flat_library_to_json: unsupported type %d", lib->type);
	}
}

static void flat_library_from_json(cJSON *j, struct flat_library *lib, int version)
{
	switch (lib->type) {
	case FLAT_LIB_TIMELINE: {
		cJSON *a = json_get_array(j, "timelines");
		int n = cJSON_GetArraySize(a);
		lib->timeline.nr_timelines = n;
		lib->timeline.timelines = xcalloc(n, sizeof(struct flat_timeline));
		for (int i = 0; i < n; i++)
			timeline_from_json(cJSON_GetArrayItem(a, i),
			                   &lib->timeline.timelines[i], version);
		break;
	}
	case FLAT_LIB_STOP_MOTION:
		stop_motion_from_json(j, &lib->stop_motion);
		break;
	case FLAT_LIB_EMITTER:
		emitter_from_json(j, &lib->emitter);
		break;
	default:
		ALICE_ERROR("flat_library_from_json: unsupported type %d", lib->type);
	}
}

static void buffer_write_file(struct buffer *buf, const char *path)
{
	size_t len;
	uint8_t *data = file_read(path, &len);
	if (!data)
		ALICE_ERROR("reading '%s': %s", path, strerror(errno));
	buffer_write_bytes(buf, data, len);
	free(data);
}

static struct string *get_path(const struct string *dir, const char *file)
{
	char *ufile = conv_output_utf8(file);
	struct string *path = string_path_join(dir, ufile);
	free(ufile);
	return path;
}

static void buffer_write_file_relative(struct buffer *buf, const struct string *dir, const char *name)
{
	struct string *path = get_path(dir, name);
	buffer_write_file(buf, path->text);
	free_string(path);
}

static void pad_align(struct buffer *b)
{
	while (b->index & 3) {
		uint8_t zero = 0;
		buffer_write_bytes(b, &zero, 1);
	}
}

static void deserialize_binary(struct buffer *b, struct string *s)
{
	if (s->size % 2 != 0)
		ALICE_ERROR("Serialized binary data has odd size");

	size_t off = b->index;
	buffer_skip(b, 4);

	for (int i = 0; i < s->size; i += 2) {
		uint8_t n;
		sscanf(s->text+i, "%02hhx", &n);
		buffer_write_bytes(b, &n, 1);
	}

	unsigned size = b->index - off - 4;
	buffer_write_int32_at(b, off, size);
	if (size & 3) {
		int npad = 4 - (size & 3);
		buffer_write_bytes(b, (const uint8_t*)"\0\0\0", npad);
	}
}

static bool libl_type_is_json(int type)
{
	return type == FLAT_LIB_TIMELINE
	    || type == FLAT_LIB_STOP_MOTION
	    || type == FLAT_LIB_EMITTER;
}

static void write_libl_files(struct buffer *b, struct ex_table *libl, const struct string *dir,
                             bool elna, int version)
{
	// validate fields
	if (libl->nr_fields != 5)
		ALICE_ERROR("Wrong number of columns in 'libl' table");
	if (libl->fields[0].type != EX_STRING)
		ALICE_ERROR("Wrong type for column 'name' in 'libl' table");
	if (libl->fields[1].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'type' in 'libl' table");
	if (libl->fields[2].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'has_front' in 'libl' table");
	if (libl->fields[3].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'front' in 'libl' table");
	if (libl->fields[4].type != EX_STRING)
		ALICE_ERROR("Wrong type");

	buffer_write_int32(b, libl->nr_rows);

	for (unsigned i = 0; i < libl->nr_rows; i++) {
		size_t off = b->index;
		const char *sjis_name = libl->rows[i][0].s->text;
		size_t name_len = strlen(sjis_name);
		buffer_write_int32(b, name_len);
		if (elna) {
			for (size_t k = 0; k < name_len; k++)
				buffer_write_int8(b, sjis_name[k] ^ 0x55);
		} else {
			buffer_write_bytes(b, (const uint8_t*)sjis_name, name_len);
		}
		pad_align(b);
		int32_t type = libl->rows[i][1].i;
		buffer_write_int32(b, type);

		struct string *path = get_path(dir, libl->rows[i][4].s->text);

		if (libl_type_is_json(type)) {
			cJSON *j = json_parse_file(path->text);
			struct flat_library lib = {0};
			lib.type = type;
			flat_library_from_json(j, &lib, version);
			cJSON_Delete(j);

			struct buffer payload;
			buffer_init(&payload, NULL, 0);
			flat_write_library_payload(&payload, &lib, version);

			if (elna && (type == FLAT_LIB_STOP_MOTION || type == FLAT_LIB_EMITTER)) {
				for (size_t k = 0; k < payload.index; k++)
					payload.buf[k] ^= 0x55;
			}

			buffer_write_int32(b, payload.index);
			buffer_write_bytes(b, payload.buf, payload.index);
			free(payload.buf);
		} else {
			// CG / MEMORY: raw file with optional generate_mipmap prefix
			if (libl->rows[i][2].i) {
				buffer_write_int32(b, file_size(path->text) + 4);
				buffer_write_int32(b, libl->rows[i][3].i);
			} else {
				buffer_write_int32(b, file_size(path->text));
			}
			buffer_write_file(b, path->text);
		}
		free_string(path);
		unsigned size = b->index - off;
		if (size & 3) {
			int npad = 4 - (size & 3);
			buffer_write_bytes(b, (const uint8_t*)"\0\0\0", npad);
		}
	}
}

static void write_talt_files(struct buffer *b, struct ex_table *talt, const struct string *dir)
{
	// validate fields
	if (talt->nr_fields != 2)
		ALICE_ERROR("Wrong number of columns in 'talt' table");
	if (talt->fields[0].type != EX_STRING)
		ALICE_ERROR("Wrong type for column 'unknown' in 'talt' table");
	if (talt->fields[1].type != EX_TABLE)
		ALICE_ERROR("Wrong type for column 'meta' in 'talt' table");
	if (talt->fields[1].nr_subfields != 5)
		ALICE_ERROR("Wrong number of columns in 'talt.meta' table");
	if (talt->fields[1].subfields[0].type != EX_STRING)
		ALICE_ERROR("Wrong type for column 'uk1' in 'talt.meta' table");
	if (talt->fields[1].subfields[1].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'uk2' in 'talt.meta' table");
	if (talt->fields[1].subfields[2].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'uk3' in 'talt.meta' table");
	if (talt->fields[1].subfields[3].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'uk4' in 'talt.meta' table");
	if (talt->fields[1].subfields[4].type != EX_INT)
		ALICE_ERROR("Wrong type for column 'uk5' in 'talt.meta' table");

	buffer_write_int32(b, talt->nr_rows);

	for (unsigned i = 0; i < talt->nr_rows; i++) {
		struct string *path = get_path(dir, talt->rows[i][0].s->text);
		buffer_write_int32(b, file_size(path->text));
		buffer_write_file(b, path->text);
		free_string(path);
		pad_align(b);

		struct ex_table *meta = talt->rows[i][1].t;
		buffer_write_int32(b, meta->nr_rows);
		for (unsigned i = 0; i < meta->nr_rows; i++) {
			deserialize_binary(b, meta->rows[i][0].s);
			buffer_write_int32(b, meta->rows[i][1].i);
			buffer_write_int32(b, meta->rows[i][2].i);
			buffer_write_int32(b, meta->rows[i][3].i);
			buffer_write_int32(b, meta->rows[i][4].i);
		}
	}
}

static struct flat *build_flat(struct ex *ex, const struct string *dir)
{
	struct buffer b;
	struct flat *flat = xcalloc(1, sizeof(struct flat));
	buffer_init(&b, NULL, 0);

	bool elna = ex_get_int(ex, "elna", 0);
	if (elna) {
		flat->elna.present = true;
		flat->elna.off = b.index;
		flat->elna.size = 0;
		buffer_write_bytes(&b, (uint8_t*)"ELNA", 4);
		buffer_write_int32(&b, 0);
	}

	// FLAT section
	struct string *flat_path = ex_get_string(ex, "flat");
	if (!flat_path)
		ALICE_ERROR("'flat' path missing from .flat manifest");
	struct string *flat_full = get_path(dir, flat_path->text);
	cJSON *head_j = json_parse_file(flat_full->text);
	free_string(flat_full);
	free_string(flat_path);
	struct flat_header hdr;
	flat_header_from_json(head_j, &hdr);
	cJSON_Delete(head_j);

	flat->flat.present = true;
	flat->flat.off = b.index;
	buffer_write_bytes(&b, (uint8_t*)"FLAT", 4);
	buffer_write_int32(&b, 0);
	flat_write_header(&b, &hdr);
	flat->flat.size = b.index - flat->flat.off - 8;
	buffer_write_int32_at(&b, flat->flat.off + 4, flat->flat.size);
	int version = hdr.version;

	// TMNL section (raw passthrough)
	struct string *tmnl_path = ex_get_string(ex, "tmnl");
	if (tmnl_path) {
		flat->tmnl.present = true;
		flat->tmnl.off = b.index;
		buffer_write_file_relative(&b, dir, tmnl_path->text);
		flat->tmnl.size = b.index - flat->tmnl.off - 8;
		free_string(tmnl_path);
	}

	// MTLC section
	struct string *mtlc_path = ex_get_string(ex, "mtlc");
	if (!mtlc_path)
		ALICE_ERROR("'mtlc' path missing from .flat manifest");
	struct string *mtlc_full = get_path(dir, mtlc_path->text);
	cJSON *mtlc_j = json_parse_file(mtlc_full->text);
	free_string(mtlc_full);
	free_string(mtlc_path);
	struct flat_timeline *timelines;
	size_t nr_timelines;
	flat_mtlc_from_json(mtlc_j, &timelines, &nr_timelines, version);
	cJSON_Delete(mtlc_j);

	flat->mtlc.present = true;
	flat->mtlc.off = b.index;
	buffer_write_bytes(&b, (uint8_t*)"MTLC", 4);
	buffer_write_int32(&b, 0);
	flat_write_timelines(&b, timelines, nr_timelines, version);
	flat->mtlc.size = b.index - flat->mtlc.off - 8;
	buffer_write_int32_at(&b, flat->mtlc.off + 4, flat->mtlc.size);

	struct ex_table *libl = ex_get_table(ex, "libl");
	if (!libl)
		ALICE_ERROR("'libl' table missing from .flat manifest");
	flat->libl.present = true;
	flat->libl.off = b.index;
	buffer_write_bytes(&b, (uint8_t*)"LIBL", 4);
	buffer_write_int32(&b, 0);
	write_libl_files(&b, libl, dir, elna, version);
	// write the section size now that we know it
	buffer_write_int32_at(&b, flat->libl.off + 4, b.index - flat->libl.off - 8);

	struct ex_table *talt = ex_get_table(ex, "talt");
	if (talt) {
		flat->talt.present = true;
		flat->talt.off = b.index;
		buffer_write_bytes(&b, (uint8_t*)"TALT", 4);
		buffer_write_int32(&b, 0);
		write_talt_files(&b, talt, dir);
		// write the section size now that we know it
		buffer_write_int32_at(&b, flat->talt.off + 4, b.index - flat->talt.off - 8);
	}

	flat->data_size = b.index;
	flat->data = b.buf;
	flat->needs_free = true;
	return flat;
}

struct flat *flat_build(const char *xpath, struct string **output_path)
{
	struct string *dir = cstr_to_string(path_dirname(xpath));
	struct ex *ex = ex_parse_file(xpath);
	if (!ex) {
		ALICE_ERROR("Failed to read flat manifest file: %s", xpath);
	}

	if (output_path) {
		// FIXME: this sucks
		struct string *tmp = ex_get_string(ex, "output");
		if (tmp) {
			char *utmp = conv_output_utf8(tmp->text);
			*output_path = cstr_to_string(utmp);
			free(utmp);
			free_string(tmp);
		}
	}

	struct flat *flat = build_flat(ex, dir);
	free_string(dir);
	ex_free(ex);
	return flat;
}

static const char *libl_get_extension(struct flat *flat, struct flat_library *lib)
{
	switch (lib->type) {
	case FLAT_LIB_CG:
		return cg_file_extension(cg_check_format((uint8_t*)lib->cg.data));
	case FLAT_LIB_MEMORY:
		return "mem";
	case FLAT_LIB_TIMELINE:
		return "tln.json";
	case FLAT_LIB_STOP_MOTION:
		return "smo.json";
	case FLAT_LIB_EMITTER:
		return "emi.json";
	}
	return "dat";
}

static const char *talt_get_extension(struct flat *flat, struct talt_entry *e)
{
	return cg_file_extension(cg_check_format(flat->data + e->off));
}

static char *serialize_bytes(const uint8_t *b, size_t size)
{
	char *out = xmalloc(size * 2 + 1);
	for (unsigned i = 0; i < size; i++) {
		sprintf(out + i*2, "%02x", (unsigned)b[i]);
	}
	out[size*2] = '\0';
	return out;
}

static void write_file(const char *path, void *data, size_t size)
{
	if (!file_write(path, data, size))
		ALICE_ERROR("file_write(\"%s\"): %s", path, strerror(errno));
}

static void write_cg(const char *path, void *data, size_t size, bool png)
{
	enum cg_type cg_type = cg_check_format(data);
	if (cg_type == ALCG_UNKNOWN) {
		WARNING("Unknown CG format for %s", path);
		write_file(path, data, size);
	} else if (!png || cg_type == ALCG_PNG) {
		write_file(path, data, size);
	} else {
		// convert to PNG
		struct cg *cg = cg_load_buffer(data, size);
		if (!cg) {
			WARNING("Failed to decode cg for %s", path);
			write_file(path, data, size);
		} else {
			FILE *f = checked_fopen(path, "wb");
			cg_write(cg, ALCG_PNG, f);
			fclose(f);
		}
		cg_free(cg);
	}
}

static void write_section(const char *path, struct flat *flat, struct flat_section *section)
{
	write_file(path, flat->data + section->off, section->size + 8);
}

void flat_extract(struct flat *flat, const char *output_file, bool png)
{
	FILE *out = checked_fopen(output_file, "wb");
	char *prefix = escape_string_noconv(path_basename(output_file));
	char path_buf[PATH_MAX];
	int version = flat->hdr.version;

	// ELNA section
	fprintf(out, "int elna = %d;\n\n", flat->elna.present ? 1 : 0);

	// FLAT section -> JSON
	fprintf(out, "string flat = \"%s.head.json\";\n\n", prefix);
	snprintf(path_buf, PATH_MAX-1, "%s.head.json", output_file);
	if (!flat->hdr.present)
		ALICE_ERROR("FLAT header missing or unrecognized");
	cJSON *head_j = flat_header_to_json(&flat->hdr);
	json_write_file(path_buf, head_j);
	cJSON_Delete(head_j);

	// TMNL section (raw passthrough)
	if (flat->tmnl.present) {
		fprintf(out, "string tmnl = \"%s.tmnl\";\n\n", prefix);
		snprintf(path_buf, PATH_MAX-1, "%s.tmnl", output_file);
		write_section(path_buf, flat, &flat->tmnl);
	}

	// MTLC section -> JSON
	fprintf(out, "string mtlc = \"%s.mtlc.json\";\n\n", prefix);
	snprintf(path_buf, PATH_MAX-1, "%s.mtlc.json", output_file);
	cJSON *mtlc_j = flat_mtlc_to_json(flat->timelines, flat->nr_timelines, version);
	json_write_file(path_buf, mtlc_j);
	cJSON_Delete(mtlc_j);

	// LIBL section
	fprintf(out, "table libl = {\n");
	fprintf(out, "\t{ string name, int type, int has_front, int front, string path },\n");
	for (unsigned i = 0; i < flat->nr_libraries; i++) {
		struct flat_library *lib = &flat->libraries[i];
		const char *ext = (png && lib->type == FLAT_LIB_CG) ? "png" : libl_get_extension(flat, lib);
		char *name = escape_string(lib->name->text);
		bool have_generate_mipmap = lib->type == FLAT_LIB_CG && flat->hdr.version > 0;
		int32_t generate_mipmap = have_generate_mipmap ? lib->cg.generate_mipmap : 0;
		fprintf(out, "\t{ \"%s\", %d, %d, %d, \"%s.libl.%d.%s\" },\n",
			name, lib->type, have_generate_mipmap, generate_mipmap, prefix, i, ext);
		free(name);

		// write payload file
		snprintf(path_buf, PATH_MAX-1, "%s.libl.%d.%s", output_file, i, ext);
		if (lib->type == FLAT_LIB_CG) {
			write_cg(path_buf, (uint8_t*)lib->cg.data, lib->cg.size, png);
		} else if (libl_type_is_json(lib->type)) {
			cJSON *lib_j = flat_library_to_json(lib, version);
			json_write_file(path_buf, lib_j);
			cJSON_Delete(lib_j);
		} else {
			// MEMORY / unknown -> raw passthrough
			write_file(path_buf, flat->data + lib->payload_off, lib->size);
		}

	}
	fprintf(out, "};\n");

	// TALT section
	if (flat->talt.present) {
		fprintf(out, "\ntable talt = {\n");
		fprintf(out, "\t{ string path, table meta { string uk1, int uk2, int uk3, int uk4, int uk5 }},\n");
		for (unsigned i = 0; i < flat->nr_talt_entries; i++) {
			struct talt_entry *e = &flat->talt_entries[i];
			const char *ext = png ? "png" : talt_get_extension(flat, e);
			fprintf(out, "\t{ \"%s.talt.%d.%s\", {\n", prefix, i, ext);
			for (unsigned j = 0; j < e->nr_meta; j++) {
				struct talt_metadata *m = &e->metadata[j];
				char *uk = serialize_bytes(flat->data + m->unknown1_off, m->unknown1_size);
				fprintf(out, "\t\t{ \"%s\", %d, %d, %d, %d },\n",
				        uk, m->unknown2, m->unknown3, m->unknown4, m->unknown5);
				free(uk);
			}
			fprintf(out, "\t}},\n");

			// write file
			snprintf(path_buf, PATH_MAX-1, "%s.talt.%d.%s", output_file, i, ext);
			write_cg(path_buf, flat->data + e->off, e->size, png);
		}
		fprintf(out, "};\n");
	}

	free(prefix);
	fclose(out);
}
