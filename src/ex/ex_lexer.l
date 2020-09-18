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
#include "ex_parser.tab.h"
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

static void begin_included_file(const char *path)
{
    // push current working directory to stack
    char *cwd = xmalloc(2048);
    cwd = getcwd(cwd, 2048);
    include_stack[include_stack_ptr++] = cwd;

    // chdir to directory of included file
    char *tmp = strdup(path);
    chdir(dirname(tmp));
    free(tmp);
}

static void end_included_file(void)
{
    // pop dir from stack and chdir to it
    include_stack_ptr--;
    chdir(include_stack[include_stack_ptr]);
    free(include_stack[include_stack_ptr]);
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
        yex_in = file_open_utf8(yex_text+1, "r");
        if (!yex_in)
            ERROR("Failed to open included file '%s': %s", yex_text, strerror(errno));
        yex_push_buffer_state(yex__create_buffer(yex_in, YY_BUF_SIZE));
        begin_included_file(yex_text+1);
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
