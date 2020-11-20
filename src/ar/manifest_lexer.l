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

extern char string_buf[65536];
extern char *string_buf_ptr;

unsigned long ar_mf_line = 1;
extern void ar_mf_error(const char *s);

%}

%option noyywrap
%option prefix="ar_mf_"

%%

[ \t\r]     ;
\n          ar_mf_line++; return NEWLINE;
,           return COMMA;
[^,\n]*     ar_mf_lval.string = make_string(ar_mf_text, ar_mf_leng); return STRING;

%%
