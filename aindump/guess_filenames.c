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

#include <stdio.h>
#include <string.h>
#include "aindump.h"
#include "kvec.h"
#include "little_endian.h"
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "system4/utfsjis.h"

struct guesser_state {
	kvec_t(struct ain_function*) functions;
	char *guess;
};

static size_t get_prefix_length(char *a, char *b)
{
	size_t i;
	for (i = 0; a[i] && b[i]; i++) {
		if (a[i] != b[i])
			break;
	}
	return i;
}

static char *guess_itoa(possibly_unused struct guesser_state *state, size_t n)
{
	char buf[512];
	snprintf(buf, 512, "%" SIZE_T_FMT "u", n);
	return strdup(buf);
}

static char *guess_from_function_name(char *name, size_t len)
{
	size_t i, dst = 0;
	char buf[512];

	for (i = 0; i < len && dst < 510; i++) {
		unsigned char c = name[i];
		if (SJIS_2BYTE(c)) {
			// check for invalid SJIS
			if (!name[i+1])
				break;
			buf[dst++] = c;
			buf[dst++] = name[++i];
		} else if (c == '@') {
			buf[dst++] = '/';
		} else if (c == ':' && name[i+1] == ':') {
			buf[dst++] = '/';
			i++;
		} else if (c < 32 || c == '<' || c == '>' || c == ':' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
			buf[dst++] = '_';
		} else {
			buf[dst++] = c;
		}
	}
	buf[dst] = '\0';
	return strdup(buf);
}

static char *guess(struct guesser_state *state, int n)
{
	if (kv_size(state->functions) == 0)
		return guess_itoa(state, n);

	char *name = kv_A(state->functions, 0)->name;
	size_t prefix = strlen(name);
	for (size_t i = 1; i < kv_size(state->functions); i++) {
		prefix = min(prefix, get_prefix_length(name, kv_A(state->functions, i)->name));
	}
	if (!prefix)
		return guess_itoa(state, n);
	if (name[prefix] == '\0')
	while (prefix > 0 && name[prefix] && name[prefix] != '@' && name[prefix] != ':') {
		prefix--;
	}
	if (prefix)
		return guess_from_function_name(name, prefix);
	return guess_itoa(state, n);
}

static void handle_instruction(struct code_reader *r, void *data)
{
	struct guesser_state *state = data;
	switch (r->instr->opcode) {
	case FUNC: {
		int n = code_reader_get_arg(r, 0);
		if (n < 0 || n >= r->ain->nr_functions)
			ERROR("Invalid function index: %d", n);
		if (!strncmp(r->ain->functions[n].name, "<lambda", 7))
			break;
		kv_push(struct ain_function*, state->functions, &r->ain->functions[n]);
		break;
	}
	case _EOF: {
		int n = code_reader_get_arg(r, 0);
		if (n < 0)
			ERROR("Invalid filename index: %d", n);
		if (n >= r->ain->nr_filenames) {
			r->ain->filenames = xrealloc_array(r->ain->filenames, r->ain->nr_filenames, n+1, sizeof(char*));
			r->ain->nr_filenames = n+1;
		}
		if (r->ain->filenames[n]) {
			WARNING("Duplicate file index: %d", n);
			free(r->ain->filenames[n]);
		}
		// TODO: ensure uniqueness of filenames
		r->ain->filenames[n] = guess(state, n);
		kv_destroy(state->functions);
		kv_init(state->functions);
		break;
	}
	default:
		break;
	}
}

static void handle_error(possibly_unused struct code_reader *r, char *msg)
{
	ERROR("%s", msg);
}

void guess_filenames(struct ain *ain)
{
	struct guesser_state state = {0};
	kv_init(state.functions);

	for_each_instruction(ain, handle_instruction, handle_error, &state);

	for (int i = 0; i < ain->nr_filenames; i++) {
		if (!ain->filenames[i]) {
			WARNING("Unset filename: %d", i);
			continue;
		}
	}

	kv_destroy(state.functions);
}
