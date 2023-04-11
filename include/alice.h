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

#ifndef ALICE_H_
#define ALICE_H_

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "system4.h"
#include "system4/file.h"

#define ALICE_TOOLS_VERSION "0.13.0"

#define ALICE_ERROR(msg, ...) sys_error("ERROR: " msg "\n", ##__VA_ARGS__)

/* conv.c */
void set_input_encoding(const char *enc);
void set_output_encoding(const char *enc);
void set_encodings(const char *input_enc, const char *output_enc);

char *conv_output(const char *str);
char *conv_output_len(const char *str, size_t len);
struct string *string_conv_output(const char *str, size_t len);

char *conv_input(const char *str);
char *conv_input_len(const char *str, size_t len);
struct string *string_conv_input(const char *str, size_t len);

char *conv_utf8(const char *str);
char *conv_utf8_len(const char *str, size_t len);
struct string *string_conv_utf8(const char *str, size_t len);

char *conv_output_utf8(const char *str);
char *conv_output_utf8_len(const char *str, size_t len);
struct string *string_conv_output_utf8(const char *str, size_t len);

char *conv_utf8_input(const char *str);
char *conv_utf8_input_len(const char *str, size_t len);
struct string *string_conv_utf8_input(const char *str, size_t len);

void conv_cmdline_utf8(int *pargc, char ***pargv);

struct stat;

/* util.c */
char *escape_string(const char *str);
char *escape_string_noconv(const char *str);
FILE *checked_fopen(const char *filename, const char *mode);
void checked_fwrite(void *ptr, size_t size, FILE *stream);
void checked_fread(void *ptr, size_t size, FILE *stream);
UDIR *checked_opendir(const char *path);
void checked_stat(const char *path, ustat *s);
void mkdir_for_file(const char *filename);
void chdir_to_file(const char *filename);
struct string *replace_extension(const char *file, const char *ext);
struct string *string_path_join(const struct string *dir, const char *rest);
bool parse_version(const char *str, int *major, int *minor);

extern unsigned long *current_line_nr;
extern const char **current_file_name;

#endif /* ALICE_H_ */
