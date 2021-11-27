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
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "alice.h"
#include "alice/ain.h"
#include "kvec.h"
#include "little_endian.h"

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
	snprintf(buf, 512, "%zu", n);
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

void ain_guess_filenames(struct ain *ain)
{
	struct guesser_state state = {0};
	kv_init(state.functions);

	struct dasm_state dasm;
	dasm_init(&dasm, NULL, ain, 0);

	for (dasm_reset(&dasm); !dasm_eof(&dasm); dasm_next(&dasm)) {
		switch (dasm.instr->opcode) {
		case FUNC: {
			int32_t n = dasm_arg(&dasm, 0);
			if (n < 0 || n >= ain->nr_functions)
				ERROR("Invalid function index: %d", n);
			if (!strncmp(ain->functions[n].name, "<lambda", 7))
				break;
			kv_push(struct ain_function*, state.functions, &ain->functions[n]);
			break;
		}
		case _EOF: {
			int32_t n = dasm_arg(&dasm, 0);
			if (n < 0)
				ERROR("Invalid filename index: %d", n);
			if (n >= ain->nr_filenames) {
				ain->filenames = xrealloc_array(ain->filenames, ain->nr_filenames, n+1, sizeof(char*));
				ain->nr_filenames = n+1;
			}
			if (ain->filenames[n]) {
				WARNING("Duplicate file index: %d", n);
				free(ain->filenames[n]);
			}
			// TODO: ensure uniqueness of filenames
			ain->filenames[n] = guess(&state, n);
			kv_destroy(state.functions);
			kv_init(state.functions);
			break;
		}
		default:
			break;
		}
	}

	kv_destroy(state.functions);
}
