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

O   [0-7]
D   [0-9]
NZ  [1-9]
L   [a-zA-Z_]
A   [a-zA-Z_0-9]
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

extern int sym_type(const char *);  /* returns type from symbol table */

static void comment(void);
static int check_type(void);

unsigned long jaf_line = 1;

#define RETURN_STRING(tok_type) yylval.string = make_string(yytext, yyleng); return tok_type
%}

%%
"/*"                                    { comment(); }
"//".*                                    { /* consume //-comment */ }

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
"floatp"				{ return(FLOATP); }
"return"				{ return(SYM_RETURN); }
"string"				{ return(STRING); }
"struct"				{ return(STRUCT); }
"switch"				{ return(SYM_SWITCH); }
"union"					{ return(UNION); }
"void"					{ return(VOID); }
"while"					{ return(WHILE); }
"bool"                                  { return BOOL; }
"__FILE__"                              { return FILE_MACRO; }
"__LINE__"                              { return LINE_MACRO; }
"__FUNC__"                              { return FUNC_MACRO; }
"__DATE__"                              { return DATE_MACRO; }
"__TIME__"                              { return TIME_MACRO; }
"ref"                                   { return SYM_REF; }
"system"                                { return SYSTEM; }
"array"					{ return ARRAY; }
"functype"                              { return FUNCTYPE; }

{L}{A}*					{ return check_type(); }

{HP}{H}+{IS}?				{ RETURN_STRING(I_CONSTANT); }
{NZ}{D}*{IS}?				{ RETURN_STRING(I_CONSTANT); }
"0"{O}*{IS}?				{ RETURN_STRING(I_CONSTANT); }
"'"([^'\\\n]|{ES})+"'"			{ yylval.string = make_string(yytext+1, yyleng-2); return C_CONSTANT; }

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
";"					{ return ';'; }
("{"|"<%")				{ return '{'; }
("}"|"%>")				{ return '}'; }
","					{ return ','; }
":"					{ return ':'; }
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
        if (c == '*')
        {
            while ((c = input()) == '*')
                ;

            if (c == '/')
                return;

            if (c == 0)
                break;
        }
    yyerror("unterminated comment");
}

static int check_type(void)
{
    switch (sym_type(yytext))
    {
    case TYPEDEF_NAME:                /* previously defined */
        RETURN_STRING(TYPEDEF_NAME);
    case ENUMERATION_CONSTANT:        /* previously defined */
        RETURN_STRING(ENUMERATION_CONSTANT);
    default:                          /* includes undefined */
        RETURN_STRING(IDENTIFIER);
    }
}
