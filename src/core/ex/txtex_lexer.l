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

%{

#pragma GCC diagnostic ignored "-Wunused-function"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include "txtex_parser.tab.h"
#include "system4.h"
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"

static char string_buf[65536];
static char *string_buf_ptr;

struct string *make_string_from_utf8(const char *str, size_t len)
{
    char *sjis = conv_output(str);
    struct string *s = make_string(sjis, strlen(sjis));
    free(sjis);
    return s;
}

char *include_stack[256];
int include_stack_ptr = 0;

struct string *path_stack[256];
int path_stack_ptr = 0;

int yex_lex_destroy(void);

struct ex *ex_parse(FILE *in, const char *basepath)
{
    assert(path_stack_ptr == 0);
    path_stack[path_stack_ptr++] = cstr_to_string(basepath);

    yex_in = in;
    yex_parse();
    // reset lexer state for subsequent calls
    yex_lex_destroy();

    assert(path_stack_ptr == 1);
    free_string(path_stack[--path_stack_ptr]);
    return ex_data;
}

static FILE *open_included_file(const char *path)
{
    assert(path_stack_ptr > 0);
    path_stack[path_stack_ptr] = string_path_join(path_stack[path_stack_ptr-1], path_dirname(path));

    struct string *fullpath = string_path_join(path_stack[path_stack_ptr-1], path);
    FILE *f = file_open_utf8(fullpath->text, "rb");
    free_string(fullpath);

    path_stack_ptr++;
    return f;
}

static void end_included_file(void)
{
    free_string(path_stack[--path_stack_ptr]);
}

unsigned long yex_line = 1;
extern void yex_error(const char *s);

%}

%option noyywrap
%option prefix="yex_"

%x str
%x incl

id_char [^ \t\r\n\(\)\{\}\[\]=,\.;\"-]
id_head [^ \t\r\n\(\)\{\}\[\]=,\.;\"-0123456789]

%%

[ \t\r]                   ;
\n                        yex_line++;
\/\/.*\n                  yex_line++;
\(                        return '(';
\)                        return ')';
\{                        return '{';
\}                        return '}';
\[                        return '[';
\]                        return ']';
=                         return '=';
,                         return ',';
;                         return ';';
-                         return '-';
int                       return INT;
float                     return FLOAT;
string                    return STRING;
table                     return TABLE;
list                      return LIST;
tree                      return TREE;
indexed                   return INDEXED;
{id_head}{id_char}*       yex_lval.s = make_string_from_utf8(yex_text, yex_leng); return CONST_STRING;
[0-9]+\.[0-9]+            yex_lval.f = strtof(yex_text, NULL); return CONST_FLOAT;
[0-9]+                    yex_lval.i = atoi(yex_text); return CONST_INT;

\"      string_buf_ptr = string_buf; BEGIN(str);

<str>{
    \" {
        BEGIN(INITIAL);
        *string_buf_ptr = '\0';
        char *sjis = conv_output(string_buf);
        yex_lval.s = make_string(sjis, strlen(sjis));
        free(sjis);
        return CONST_STRING;
    }

    \n yex_error("Unterminated string literal");

    \\n  *string_buf_ptr++ = '\n';
    \\t  *string_buf_ptr++ = '\t';
    \\r  *string_buf_ptr++ = '\r';
    \\b  *string_buf_ptr++ = '\b';
    \\f  *string_buf_ptr++ = '\f';

    \\(.|\n)  *string_buf_ptr++ = yex_text[1];

    [^\\\n\"]+ {
        char *yptr = yex_text;
        while (*yptr)
            *string_buf_ptr++ = *yptr++;
    }
}

#include    BEGIN(incl);

<incl>{
    [ \t]*         ;
    \"[^ \t\r\n]+\" {
        yex_text[yex_leng-1] = '\0';
        yex_in = open_included_file(yex_text+1);
        if (!yex_in)
            ERROR("Failed to open included file '%s': %s", yex_text+1, strerror(errno));
        yex_push_buffer_state(yex__create_buffer(yex_in, YY_BUF_SIZE));
        BEGIN(INITIAL);
    }
}

<<EOF>> {
    yex_pop_buffer_state();
    if (!YY_CURRENT_BUFFER) {
        yyterminate();
    } else {
        end_included_file();
    }
}

%%
