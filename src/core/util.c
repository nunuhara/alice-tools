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
#include <libgen.h>
#include <unistd.h>
#include "system4.h"
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"

static unsigned long _current_line_nr = 0;
static const char *_current_file_name = NULL;

/*
 * These should be set by subcommands to point to variables tracking
 * the current line-number/file being processed, so that they can be
 * referenced in generic error messages.
 */
unsigned long *current_line_nr = &_current_line_nr;
const char **current_file_name = &_current_file_name;

static char *_escape_string(const char *str, const char *escape_chars, const char *replace_chars, bool need_conv)
{
	int escapes = 0;
	char *u = need_conv ? conv_output(str) : strdup(str);

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
	return _escape_string(str, escape_chars, replace_chars, true);
}

char *escape_string_noconv(const char *str)
{
	const char escape_chars[]  = { '\\', '\"', '\n', '\r', 0 };
	const char replace_chars[] = { '\\', '\"', 'n',  'r',  0 };
	return _escape_string(str, escape_chars, replace_chars, false);
}

FILE *checked_fopen(const char *filename, const char *mode)
{
	FILE *f = file_open_utf8(filename, mode);
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
		ALICE_ERROR("fread: %s", strerror(errno));
}

UDIR *checked_opendir(const char *path)
{
	UDIR *d = opendir_utf8(path);
	if (!d)
		ALICE_ERROR("opendir(\"%s\"): %s", path, strerror(errno));
	return d;
}

void checked_stat(const char *path, ustat *s)
{
	if (stat_utf8(path, s))
		ALICE_ERROR("stat(\"%s\"): %s", path, strerror(errno));
}

void mkdir_for_file(const char *filename)
{
	char *tmp = xstrdup(filename);
	char *dir = dirname(tmp);
	mkdir_p(dir);
	free(tmp);
}

void chdir_to_file(const char *filename)
{
	char *tmp = xstrdup(filename);
	if (chdir(dirname(tmp)))
		ALICE_ERROR("chdir(%s): %s", filename, strerror(errno));
	free(tmp);
}

/*
 * replace_extension("filename", "ext") -> "filename.ext"
 * replace_extension("filename.oth", "ext") -> "filename.ext"
 * replace_extension("filename.ext.oth", "ext") -> "filename.ext"
 */
struct string *replace_extension(const char *file, const char *ext)
{
	const char *src_ext = strrchr(file, '.');
	if (!src_ext) {
		struct string *dst = make_string(file, strlen(file));
		string_push_back(&dst, '.');
		string_append_cstr(&dst, ext, strlen(ext));
		return dst;
	}

	//size_t base_len = (src_ext+1) - file;
	size_t base_len = src_ext - file;
	struct string *dst = make_string(file, base_len);

	// handle the case where stripping the extension produces a file name
	// with the correct extension
	src_ext = strrchr(dst->text, '.');
	if (src_ext && !strcasecmp(src_ext+1, ext)) {
		return dst;
	}

	string_append_cstr(&dst, ".", 1);
	string_append_cstr(&dst, ext, strlen(ext));
	return dst;
}

struct string *string_path_join(const struct string *dir, const char *file)
{
	struct string *path = string_dup(dir);
	if (dir->size > 0 && dir->text[dir->size-1] != '/')
		string_push_back(&path, '/');
	string_append_cstr(&path, file, strlen(file));

	// fix windows paths
	for (int i = 0; i < path->size; i++) {
		if (path->text[i] == '\\')
			path->text[i] = '/';
	}
	return path;
}

bool parse_version(const char *str, int *major, int *minor)
{
	char major_str[3];
	char minor_str[3];
	const char *dot = strchr(str, '.');

	if (dot) {
		if (dot - str > 2)
			return false;
		if (strlen(dot+1) > 2)
			return false;
		strncpy(major_str, str, dot - str);
		major_str[dot-str] = 0;
		strcpy(minor_str, dot+1);
	} else {
		if (strlen(str) > 2)
			return false;
		strcpy(major_str, str);
		strcpy(minor_str, "0");
	}

	*major = atoi(major_str);
	*minor = atoi(minor_str);
	return true;
}
