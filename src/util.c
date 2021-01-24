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
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "system4.h"
#include "alice.h"

static char *_escape_string(const char *str, const char *escape_chars, const char *replace_chars)
{
	int escapes = 0;
	char *u = conv_output(str);

	// count number of required escapes
	for (int i = 0; u[i]; i++) {
		if (u[i] & 0x80)
			continue;
		for (int j = 0; escape_chars[j]; j++) {
			if (u[i] == escape_chars[j]) {
				escapes++;
				break;
			}
		}
	}

	// add backslash escapes
	if (escapes > 0) {
		int dst = 0;
		int len = strlen(u);
		char *tmp = xmalloc(len + escapes + 1);
		for (int i = 0; u[i]; i++) {
			bool escaped = false;
			for (int j = 0; escape_chars[j]; j++) {
				if (u[i] == escape_chars[j]) {
					tmp[dst++] = '\\';
					tmp[dst++] = replace_chars[j];
					escaped = true;
					break;
				}
			}
			if (!escaped)
				tmp[dst++] = u[i];
		}
		tmp[len+escapes] = '\0';
		free(u);
		u = tmp;
	}

	return u;
}

char *escape_string(const char *str)
{
	const char escape_chars[]  = { '\\', '\"', '\n', '\r', 0 };
	const char replace_chars[] = { '\\', '\"', 'n',  'r',  0 };
	return _escape_string(str, escape_chars, replace_chars);
}

FILE *checked_fopen(const char *filename, const char *mode)
{
	FILE *f = fopen(filename, mode);
	if (!f)
		ALICE_ERROR("fopen(\"%s\", \"%s\"): %s", filename, mode, strerror(errno));
	return f;
}

void checked_fwrite(void *ptr, size_t size, FILE *stream)
{
	if (fwrite(ptr, size, 1, stream) != 1)
		ALICE_ERROR("fwrite: %s", strerror(errno));
}

void checked_fread(void *ptr, size_t size, FILE *stream)
{
	if (fread(ptr, size, 1, stream) != 1)
		ALICE_ERROR("fwrite: %s", strerror(errno));
}

DIR *checked_opendir(const char *path)
{
	DIR *d = opendir(path);
	if (!d)
		ALICE_ERROR("opendir(\"%s\"): %s", path, strerror(errno));
	return d;
}

void checked_stat(const char *path, struct stat *s)
{
	if (stat(path, s))
		ALICE_ERROR("stat(\"%s\"): %s", path, strerror(errno));
}
