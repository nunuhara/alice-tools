%define api.prefix {yex_}

%union {
    int token;
    int i;
    float f;
    struct string *s;
    struct ex *ex;
    struct ex_block *block;
    struct ex_table *table;
    struct ex_field *field;
    struct ex_list *list;
    struct ex_tree *tree;
    struct ex_value *value;
    block_list *blocks;
    field_list *fields;
    row_list *rows;
    value_list *values;
    node_list *nodes;
}

%code requires {
    #include "kvec.h"
    #include "system4/ex.h"

    kv_decl(block_list, struct ex_block*);
    kv_decl(field_list, struct ex_field*);
    kv_decl(value_list, struct ex_value*);
    kv_decl(row_list,   value_list*);
    kv_decl(node_list,  struct ex_tree*);

    struct ex *ex_data;

}

%{

#include <stdio.h>
#include "system4.h"
#include "system4/ex.h"
#include "system4/string.h"
#include "ast.h"

extern int yex_lex();
extern unsigned long yex_line;

void yex_error(const char *s)
{
    sys_error("ERROR: at line %lu: %s\n", yex_line, s);
}

%}

%token	<token>		INT FLOAT STRING TABLE LIST TREE INDEXED
%token	<i>		CONST_INT
%token	<f>		CONST_FLOAT
%token	<s>		CONST_STRING

%type	<ex>		exdata
%type	<blocks>	stmts
%type	<block>		stmt
%type	<table>		table
%type	<fields>	header
%type	<fields>	fields
%type	<field>		field
%type	<rows>		body
%type	<rows>		rows
%type	<values>	row
%type	<values>	cells
%type	<value>		cell
%type	<list>		list
%type	<values>	values
%type	<value>		value
%type	<tree>		tree
%type	<nodes>		nodes
%type	<tree>		node
%type	<token>		ftype
%type	<i>		int
%type	<f>		float

%start exdata

%%

exdata	:	stmts { ex_data = ast_make_ex($1); }
	;

stmts	:	stmt ';'       { $$ = ast_make_block_list($1); }
	|	stmts stmt ';' { $$ = ast_block_list_push($1, $2); }
	;

stmt	:	INT    CONST_STRING '=' int          { $$ = ast_make_int_block($2, $4); }
	|	FLOAT  CONST_STRING '=' float        { $$ = ast_make_float_block($2, $4); }
	|	STRING CONST_STRING '=' CONST_STRING { $$ = ast_make_string_block($2, $4); }
	|	TABLE  CONST_STRING '=' table        { $$ = ast_make_table_block($2, $4); }
	|	LIST   CONST_STRING '=' list         { $$ = ast_make_list_block($2, $4); }
	|	TREE   CONST_STRING '=' tree         { $$ = ast_make_tree_block($2, $4); }
	;

int	:	CONST_INT     { $$ =   $1; }
	|	'-' CONST_INT { $$ = - $2; }
	;

float	:	CONST_FLOAT     { $$ =   $1; }
	|	'-' CONST_FLOAT { $$ = - $2; }
	;

table	:	'{' header ',' rows     '}' { $$ = ast_make_table($2, $4); }
	|	'{' header ',' rows ',' '}' { $$ = ast_make_table($2, $4); }
	;

header	:	'{' fields     '}' { $$ = $2; }
	|	'{' fields ',' '}' { $$ = $2; }
	;

fields	:	field            { $$ = ast_make_field_list($1); }
	|	fields ',' field { $$ = ast_field_list_push($1, $3); }
	;

field	:	TABLE CONST_STRING header                      { $$ = ast_make_field(EX_TABLE, $2, NULL, false, $3); }
	|	INDEXED ftype CONST_STRING                     { $$ = ast_make_field($2,       $3, NULL, true,  NULL); }
	|	INDEXED ftype CONST_STRING '=' value           { $$ = ast_make_field($2,       $3, $5,   true,  NULL); }
	|	ftype CONST_STRING                             { $$ = ast_make_field($1,       $2, NULL, false, NULL); }
	|	ftype CONST_STRING '=' value                   { $$ = ast_make_field($1,       $2, $4,   false, NULL); }
	// DEPRECATED BELOW
	|	ftype CONST_STRING '[' int ',' int ']'         { $$ = ast_make_field_old($1, $2, $4, $6, 0,  NULL); }
	|	ftype CONST_STRING '[' int ',' int ',' int ']' { $$ = ast_make_field_old($1, $2, $4, $6, $8, NULL); }
	;

ftype	:	INT    { $$ = EX_INT; }
	|	FLOAT  { $$ = EX_FLOAT; }
	|	STRING { $$ = EX_STRING; }
	|	LIST   { $$ = EX_LIST; }
	|	TREE   { $$ = EX_TREE; }
	;

body	:	'{'          '}' { $$ = NULL; }
	|	'{' rows     '}' { $$ = $2; }
	|	'{' rows ',' '}' { $$ = $2; }
	;

rows	:	row          { $$ = ast_make_row_list($1); }
	|	rows ',' row { $$ = ast_row_list_push($1, $3); }
	;

row	:	'{' cells     '}' { $$ = $2; }
	|	'{' cells ',' '}' { $$ = $2; }
	;

cells	:	cell           { $$ = ast_make_value_list($1); }
	|	cells ',' cell { $$ = ast_value_list_push($1, $3); }
	;

cell	:	value { $$ = $1; }
	|	body  { $$ = ast_make_subtable($1); }
	;

list	:	'{'            '}' { $$ = ast_make_list(NULL); }
	|	'{' values     '}' { $$ = ast_make_list($2); }
	|	'{' values ',' '}' { $$ = ast_make_list($2); }
	;

values	:	value            { $$ = ast_make_value_list($1); }
	|	values ',' value { $$ = ast_value_list_push($1, $3); }
	;

value	:	int                 { $$ = ast_make_int($1); }
	|	float               { $$ = ast_make_float($1); }
	|	CONST_STRING        { $$ = ast_make_string($1); }
	|	'(' TABLE ')' table { $$ = ast_make_table_value($4); }
	|	'(' LIST  ')' list  { $$ = ast_make_list_value($4); }
	|	'(' TREE  ')' tree  { $$ = ast_make_tree_value($4); }
	;

tree	:	'{'           '}' { $$ = ast_make_tree(NULL); }
	|	'{' nodes     '}' { $$ = ast_make_tree($2); }
	|	'{' nodes ',' '}' { $$ = ast_make_tree($2); }
	;

nodes	:	node           { $$ = ast_make_node_list($1); }
	|	nodes ',' node { $$ = ast_node_list_push($1, $3); }
	;

node	:	CONST_STRING '=' int                 { $$ = ast_make_leaf_int($1, $3); }
	|	CONST_STRING '=' float               { $$ = ast_make_leaf_float($1, $3); }
	|	CONST_STRING '=' CONST_STRING        { $$ = ast_make_leaf_string($1, $3); }
	|	CONST_STRING '=' '(' TABLE ')' table { $$ = ast_make_leaf_table($1, $6); }
	|	CONST_STRING '=' '(' LIST  ')' list  { $$ = ast_make_leaf_list($1, $6); }
	|	CONST_STRING '=' tree                { $$ = $3; $$->name = $1; }
	;

%%
