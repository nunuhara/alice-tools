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
#include "manifest_parser.tab.h"
#include "system4.h"
#include "system4/string.h"
#include "alice.h"

static char string_buf[256];
static char *string_buf_ptr;

unsigned long ar_mf_line = 1;
extern void ar_mf_error(const char *s);

%}

%option noyywrap
%option prefix="ar_mf_"

%x str

%%

[ \t\r]        ;
\n             ar_mf_line++; return NEWLINE;
,              return COMMA;
[^,\" \t\r\n]* ar_mf_lval.string = make_string(ar_mf_text, ar_mf_leng); return STRING;
\"             string_buf_ptr = string_buf; BEGIN(str);

<str>{
    \" {
        BEGIN(INITIAL);
        *string_buf_ptr = '\0';
        ar_mf_lval.string = make_string(string_buf, strlen(string_buf));
        return STRING;
    }

    \n ar_mf_error("Unterminated string literal");

    \\n  *string_buf_ptr++ = '\n';
    \\t  *string_buf_ptr++ = '\t';
    \\r  *string_buf_ptr++ = '\r';
    \\b  *string_buf_ptr++ = '\b';
    \\f  *string_buf_ptr++ = '\f';

    \\(.|\n)  *string_buf_ptr++ = ar_mf_text[1];

    [^\\\n\"]+ {
        char *yptr = ar_mf_text;
        while (*yptr)
            *string_buf_ptr++ = *yptr++;
    }
}

%%
