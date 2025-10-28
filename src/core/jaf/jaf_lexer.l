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
 *
 * Based on ANSI C grammar from http://www.quut.com/c/ANSI-C-grammar-l-2011.html
 */

%e  1019
%p  2807
%n  371
%k  284
%a  1213
%o  1117

U   [\x80-\xbf]
U2  [\xc2-\xdf]
U3  [\xe0-\xef]
U4  [\xf0-\xf4]

UCH {U2}{U}|{U3}{U}{U}|{U4}{U}{U}{U}

O   [0-7]
D   [0-9]
NZ  [1-9]
L   ([a-zA-Z_]|{UCH})
A   ([a-zA-Z_0-9#]|{UCH})
AT  ([a-zA-Z_0-9]|{UCH})
H   [a-fA-F0-9]
HP  (0[xX])
E   ([Ee][+-]?{D}+)
P   ([Pp][+-]?{D}+)
FS  (f|F|l|L)
IS  (((u|U)(l|L|ll|LL)?)|((l|L|ll|LL)(u|U)?))
ES  (\\(['"\?\\abfnrtv]|[0-7]{1,3}|x[a-fA-F0-9]+))
WS  [ \t\v\f]

%{
#pragma GCC diagnostic ignored "-Wunused-function"
#include <stdio.h>
#include "system4/string.h"
#include "jaf_parser.tab.h"

extern void yyerror(const char *);  /* prints grammar violation message */

static void comment(void);

unsigned long jaf_line = 1;

#define RETURN_STRING(tok_type) yylval.string = make_string(yytext, yyleng); return tok_type
%}

%%
"/*"                                    { comment(); }
"//".*                                  { /* consume //-comment */ }

"break"					{ return(BREAK); }
"case"					{ return(CASE); }
"char"					{ return(CHAR); }
"const"					{ return(CONST); }
"continue"				{ return(CONTINUE); }
"default"				{ return(DEFAULT); }
"do"					{ return(DO); }
"else"					{ return(ELSE); }
"enum"					{ return(ENUM); }
"float"					{ return(FLOAT); }
"for"					{ return(FOR); }
"goto"					{ return(GOTO); }
"if"					{ return(IF); }
"int"					{ return(INT); }
"lint"					{ return(LINT); }
"intp"					{ return(INTP); }
"imain_system"				{ return(IMAIN_SYSTEM); }
"floatp"				{ return(FLOATP); }
"return"				{ return(SYM_RETURN); }
"string"				{ return(STRING); }
"struct"				{ return(STRUCT); }
"class"					{ return(STRUCT); }
"interface"				{ return(INTERFACE); }
"public"				{ return(PUBLIC); }
"private"				{ return(PRIVATE); }
"switch"				{ return(SYM_SWITCH); }
"union"					{ return(UNION); }
"void"					{ return(VOID); }
"while"					{ return(WHILE); }
"bool"                                  { return BOOL; }
"true"                                  { return SYM_TRUE; }
"false"                                 { return SYM_FALSE; }
"__FILE__"                              { return FILE_MACRO; }
"__LINE__"                              { return LINE_MACRO; }
"__FUNC__"                              { return FUNC_MACRO; }
"__DATE__"                              { return DATE_MACRO; }
"__TIME__"                              { return TIME_MACRO; }
"ref"                                   { return SYM_REF; }
"array"                                 { return ARRAY; }
"wrap"                                  { return WRAP; }
"hll_param"                             { return HLL_PARAM; }
"hll_func_71"                           { return HLL_FUNC_71; }
"hll_func"                              { return HLL_FUNC; }
"hll_struct"                            { return HLL_STRUCT; }
"hll_delegate"                          { return HLL_DELEGATE; }
"functype"                              { return FUNCTYPE; }
"delegate"                              { return DELEGATE; }
"override"                              { return OVERRIDE; }
"this"                                  { return THIS; }
"new"                                   { return SYM_NEW; }
"assert"                                { return ASSERT; }
"NULL"					{ return SYM_NULL; }

{L}                                     { RETURN_STRING(IDENTIFIER); }
{L}{A}*{AT}                             { RETURN_STRING(IDENTIFIER); }

{HP}{H}+{IS}?				{ RETURN_STRING(I_CONSTANT); }
{NZ}{D}*{IS}?				{ RETURN_STRING(I_CONSTANT); }
"0"{O}*{IS}?				{ RETURN_STRING(I_CONSTANT); }
"'"([^'\\\n]|{ES})*"'"			{ yylval.string = make_string(yytext+1, yyleng-2); return C_CONSTANT; }

{D}+{E}{FS}?				{ RETURN_STRING(F_CONSTANT); }
{D}*"."{D}+{E}?{FS}?			{ RETURN_STRING(F_CONSTANT); }
{D}+"."{E}?{FS}?			{ RETURN_STRING(F_CONSTANT); }
{HP}{H}+{P}{FS}?			{ RETURN_STRING(F_CONSTANT); }
{HP}{H}*"."{H}+{P}{FS}?			{ RETURN_STRING(F_CONSTANT); }
{HP}{H}+"."{P}{FS}?			{ RETURN_STRING(F_CONSTANT); }

(\"([^"\\\n]|{ES})*\"{WS}*)+		{ RETURN_STRING(STRING_LITERAL); }

"..."					{ return ELLIPSIS; }
">>="					{ return RIGHT_ASSIGN; }
"<<="					{ return LEFT_ASSIGN; }
"+="					{ return ADD_ASSIGN; }
"-="					{ return SUB_ASSIGN; }
"*="					{ return MUL_ASSIGN; }
"/="					{ return DIV_ASSIGN; }
"%="					{ return MOD_ASSIGN; }
"&="					{ return AND_ASSIGN; }
"^="					{ return XOR_ASSIGN; }
"|="					{ return OR_ASSIGN; }
"<-"					{ return REF_ASSIGN; }
">>"					{ return RIGHT_OP; }
"<<"					{ return LEFT_OP; }
"++"					{ return INC_OP; }
"--"					{ return DEC_OP; }
"&&"					{ return AND_OP; }
"||"					{ return OR_OP; }
"<="					{ return LE_OP; }
">="					{ return GE_OP; }
"=="					{ return EQ_OP; }
"!="					{ return NE_OP; }
"==="					{ return REQ_OP; }
"!=="					{ return RNE_OP; }
";"					{ return ';'; }
("{"|"<%")				{ return '{'; }
("}"|"%>")				{ return '}'; }
","					{ return ','; }
":"					{ return ':'; }
"::"					{ return DOUBLE_COLON; }
"="					{ return '='; }
"("					{ return '('; }
")"					{ return ')'; }
("["|"<:")				{ return '['; }
("]"|":>")				{ return ']'; }
"."					{ return '.'; }
"&"					{ return '&'; }
"!"					{ return '!'; }
"~"					{ return '~'; }
"-"					{ return '-'; }
"+"					{ return '+'; }
"*"					{ return '*'; }
"/"					{ return '/'; }
"%"					{ return '%'; }
"<"					{ return '<'; }
">"					{ return '>'; }
"^"					{ return '^'; }
"|"					{ return '|'; }
"?"					{ return '?'; }
"@"					{ return '@'; }

\n                                      { jaf_line++; }
{WS}+					{ /* whitespace separates tokens */ }
.					{ /* discard bad characters */ }

%%

int yywrap(void)        /* called at end of input */
{
    return 1;           /* terminate now */
}

static void comment(void)
{
    int c;

    while ((c = input()) != 0)
        if (c == '\n') {
            jaf_line++;
        } else if (c == '*') {
            while ((c = input()) == '*')
                ;

            if (c == '/')
                return;

            if (c == 0)
                break;
        }
    yyerror("unterminated comment");
}
