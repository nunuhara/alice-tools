%define api.prefix {ar_mf_}

%union {
    int token;
    struct string *string;
    ar_string_list *row;
    ar_row_list *rows;
}

%code requires {
    #include "system4/string.h"
    #include "alice/ar.h"
}

%{

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "system4.h"
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ar.h"

extern int ar_mf_lex();
extern unsigned long ar_mf_line;

extern FILE *ar_mf_in;
static struct ar_manifest *ar_mf_output;

void ar_mf_error(const char *s)
{
    sys_error("ERROR: At line %d: %s\n", ar_mf_line, s);
}

struct ar_manifest *ar_parse_manifest(const char *path)
{
    if (!strcmp(path, "-"))
	ar_mf_in = stdin;
    else
	ar_mf_in = file_open_utf8(path, "rb");
    if (!ar_mf_in)
	ALICE_ERROR("Opening input file '%s': %s", path, strerror(errno));
    ar_mf_parse();
    return ar_mf_output;
}

static ar_string_list *push_string(ar_string_list *list, struct string *str)
{
    kv_push(struct string*, *list, str);
    return list;
}

static ar_string_list *make_string_list(struct string *str)
{
    ar_string_list *list = xmalloc(sizeof(ar_string_list));
    kv_init(*list);
    return str ? push_string(list, str) : list;
}

static ar_row_list *push_row(ar_row_list *rows, ar_string_list *row)
{
    kv_push(ar_string_list*, *rows, row);
    return rows;
}

static ar_row_list *make_row_list(ar_string_list *row)
{
    ar_row_list *rows = xmalloc(sizeof(ar_row_list));
    kv_init(*rows);
    return push_row(rows, row);
}

%}

%token	<string>	STRING
%token	<token>		NEWLINE COMMA

%type	<row>		row values options
%type	<rows>		rows

%start file

%%

file    :	STRING options NEWLINE STRING rows end { ar_mf_output = ar_make_manifest($1, $2, $4, $5); }
	;

options :			{ $$ = make_string_list(NULL); }
	|	options	STRING	{ $$ = push_string($1, $2); }
	;

rows	:	row      { $$ = make_row_list($1); }
	|	rows row { $$ = push_row($1, $2); }
	;

row	:       NEWLINE values { $$ = $2; }
	;

values	:	STRING              { $$ = make_string_list($1); }
	|	values COMMA STRING { $$ = push_string($1, $3); }
	;


end	:
	|	NEWLINE end
	;

%%
