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
#include <errno.h>
#include "alice.h"
#include "system4.h"
#include "system4/ain.h"
#include "system4/file.h"
#include "system4/string.h"
#include "system4/vector.h"
#include "text_parser.tab.h"

extern FILE *text_in;
extern unsigned long text_line;
struct ain *text_ain;

void ain_read_text(const char *filename, struct ain *ain)
{
	current_line_nr = &text_line;
	current_file_name = &filename;
	text_ain = ain;

	if (!strcmp(filename, "-"))
		text_in = stdin;
	else
		text_in = file_open_utf8(filename, "r");
	if (!text_in)
		ERROR("Opening input file '%s': %s", filename, strerror(errno));
	text_parse();

	for (size_t i = 0; i < vector_length(*statements); i++) {
		struct text_assignment *assign = vector_A(*statements, i);
		if (assign->type == STRINGS) {
			if (assign->index < 0 || assign->index >= ain->nr_strings)
				ERROR("Invalid string index: %d", assign->index);
			free_string(ain->strings[assign->index]);
			ain->strings[assign->index] = assign->string;
		} else if (assign->type == MESSAGES) {
			if (assign->index < 0 || assign->index >= ain->nr_messages)
				ERROR("Invalid message index: %d", assign->index);
			free_string(ain->messages[assign->index]);
			ain->messages[assign->index] = assign->string;
		} else {
			ERROR("Unknown assignment type: %d", assign->type);
		}

		free(assign);
	}

	vector_destroy(*statements);
}
