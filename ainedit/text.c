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
#include "system4.h"
#include "system4/ain.h"
#include "system4/string.h"
#include "text_parser.tab.h"

extern FILE *text_in;

void read_text(const char *filename, struct ain *ain)
{
	if (filename) {
		if (!strcmp(filename, "-"))
			text_in = stdin;
		else
			text_in = fopen(filename, "r");
		if (!text_in)
			ERROR("Opening input file '%s': %s", filename, strerror(errno));
	}
	text_parse();

	for (size_t i = 0; i < kv_size(*statements); i++) {
		struct text_assignment *assign = kv_A(*statements, i);
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

	kv_destroy(*statements);
}
