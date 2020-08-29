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
#include "text_parser.tab.h"
#include "ainedit.h"
#include "system4.h"
#include "system4/string.h"

char string_buf[65536];
char *string_buf_ptr;

unsigned long text_line = 1;
extern void text_error(const char *s);

%}

%option noyywrap
%option prefix="text_"

%x str

%%

[ \t]                     ;
;[^\n]*\n                 text_line++; return NEWLINE;
\n                        text_line++; return NEWLINE;
\[                        return LBRACKET;
\]                        return RBRACKET;
=                         return EQUAL;
[0-9]+                    text_lval.integer = atoi(text_text); return NUMBER;
m                         return MESSAGES;
s                         return STRINGS;

\"      string_buf_ptr = string_buf; BEGIN(str);

<str>{
    \" {
        BEGIN(INITIAL);
        *string_buf_ptr = '\0';
        char *sjis = encode_text(string_buf);
        text_lval.string = make_string(sjis, strlen(sjis));
        free(sjis);
        return STRING;
    }

    \n text_error("Unterminated string literal");

    \\n  *string_buf_ptr++ = '\n';
    \\t  *string_buf_ptr++ = '\t';
    \\r  *string_buf_ptr++ = '\r';
    \\b  *string_buf_ptr++ = '\b';
    \\f  *string_buf_ptr++ = '\f';

    \\(.|\n)  *string_buf_ptr++ = text_text[1];

    [^\\\n\"]+ {
        char *yptr = text_text;
        while (*yptr)
            *string_buf_ptr++ = *yptr++;
    }
}

. return INVALID_TOKEN;

%%
