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

#ifndef AINDUMP_H
#define AINDUMP_H

#include <stdint.h>
#include <stdio.h>

struct ain;
struct ain_function;

enum {
	DASM_RAW = 1,
	DASM_NO_MACROS = 2,
};

struct code_reader {
	struct ain *ain;
	size_t addr;
	const struct instruction *instr;
};


// guess_filenames.c
void guess_filenames(struct ain *ain);

// dasm.c
void disassemble_ain(FILE *out, struct ain *ain, unsigned int flags);;
char *escape_string(const char *str);

// aindump.c
void ain_dump_function(FILE *out, struct ain *ain, struct ain_function *f);
int32_t code_reader_get_arg(struct code_reader *r, int n);
void for_each_instruction(struct ain *ain, void(*fun)(struct code_reader*, void*), void(*err)(struct code_reader*, char*), void *data);

// json.c
void ain_dump_json(FILE *out, struct ain *ain);

char *encode_text_output(const char *str);
char *encode_text_utf8(const char *str);

#endif /* AINDUMP_H */
