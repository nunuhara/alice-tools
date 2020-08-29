%define api.prefix {text_}

%union {
    int token;
    int integer;
    struct string *string;
    struct text_assignment *assign;
    assignment_list *program;
}

%code requires {
    #include "kvec.h"

    kv_decl(assignment_list, struct text_assignment*);

    struct text_assignment {
	int type;
	int index;
	struct string *string;
    };

    assignment_list *statements;

}

%{

#include <stdio.h>
#include "system4.h"
#include "system4/string.h"

extern int text_lex();
extern unsigned long text_line;

void text_error(const char *s)
{
    sys_error("ERROR: At line %lu: %s\n", text_line, s);
}

static assignment_list *make_program(void)
{
    assignment_list *program = xmalloc(sizeof(assignment_list));
    kv_init(*program);
    return program;
}

static void push_statement(assignment_list *program, struct text_assignment *statement)
{
    kv_push(struct text_assignment*, *program, statement);
}

static struct text_assignment *make_assignment(int type, int index, struct string *string)
{
    struct text_assignment *assign = xmalloc(sizeof(struct text_assignment));
    assign->type = type;
    assign->index = index;
    assign->string = string;
    return assign;
}

%}

%token	<token>		NEWLINE LBRACKET RBRACKET EQUAL MESSAGES STRINGS INVALID_TOKEN
%token	<integer>	NUMBER
%token	<string>	STRING

%type	<program>	stmts
%type	<assign>	stmt

%start program

%%

program : 	stmts { statements = $1; }
	;

stmts   :	stmt { $$ = make_program(); if ($1) { push_statement($$, $1); } }
	|	stmts stmt { if ($2) { push_statement($1, $2); } }
	;

stmt    :	NEWLINE { $$ = NULL; }
	|	MESSAGES LBRACKET NUMBER RBRACKET EQUAL STRING NEWLINE { $$ = make_assignment(MESSAGES, $3, $6); }
	|	STRINGS  LBRACKET NUMBER RBRACKET EQUAL STRING NEWLINE { $$ = make_assignment(STRINGS,  $3, $6); }
	;

%%
