%define api.prefix {csv_}

%union {
    int token;
    struct string *string;
    acx_type_list *types;
    enum acx_column_type type;
    acx_record_list *lines;
    acx_value_list *values;
    struct tagged_acx_value *value;
}

%code requires {
    #include "system4/acx.h"
    #include "kvec.h"

    struct tagged_acx_value {
	enum acx_column_type type;
	union acx_value value;
    };

    kv_decl(acx_value_list, struct tagged_acx_value*);
    kv_decl(acx_record_list, acx_value_list*);
    kv_decl(acx_type_list, enum acx_column_type);
}
%{

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "system4.h"
#include "system4/string.h"
#include "system4/utfsjis.h"

extern int csv_lex();
extern unsigned long csv_line;

FILE *csv_in;
static struct acx *acx_output;

void csv_error(const char *s)
{
    sys_error("ERROR: At line %d: %s\n", csv_line, s);
}

struct acx *acx_parse(const char *path)
{
    if (!strcmp(path, "-"))
	csv_in = stdin;
    else
	csv_in = fopen(path, "rb");
    if (!csv_in)
	ERROR("Opening input file '%s': %s", path, strerror(errno));

    csv_parse();
    return acx_output;
}

static struct acx *make_acx(acx_type_list *types, acx_record_list *lines)
{
    struct acx *acx = xcalloc(1, sizeof(struct acx));
    acx->nr_columns = kv_size(*types);
    acx->nr_lines = kv_size(*lines);
    acx->lines = xcalloc(acx->nr_lines * acx->nr_columns, sizeof(union acx_value));

    acx->column_types = kv_data(*types);
    free(types);

    for (int row = 0; row < acx->nr_lines; row++) {
	acx_value_list *line = kv_A(*lines, row);
	if (kv_size(*line) != (size_t)acx->nr_columns)
	    ERROR("Wrong number of columns at line %d (expected %d; got %d)", row, acx->nr_columns, (int)kv_size(*line));
	for (int col = 0; col < acx->nr_columns; col++) {
	    if (kv_A(*line, col)->type != acx->column_types[col])
		ERROR("Wrong value type at line %d column %d", row, col);
	    acx->lines[row*acx->nr_columns + col] = kv_A(*line, col)->value;
	    free(kv_A(*line, col));
	}
        kv_destroy(*line);
        free(line);
    }
    kv_destroy(*lines);
    free(lines);
    return acx;
}

static acx_type_list *push_type(acx_type_list *list, enum acx_column_type type)
{
    kv_push(enum acx_column_type, *list, type);
    return list;
}

static acx_type_list *make_type_list(enum acx_column_type type)
{
    acx_type_list *list = xmalloc(sizeof(acx_type_list));
    kv_init(*list);
    return push_type(list, type);
}

static acx_record_list *push_record(acx_record_list *lines, acx_value_list *line)
{
    kv_push(acx_value_list*, *lines, line);
    return lines;
}

static acx_record_list *make_record_list(acx_value_list *line)
{
    acx_record_list *lines = xmalloc(sizeof(acx_record_list));
    kv_init(*lines);
    return push_record(lines, line);
}

static acx_value_list *push_value(acx_value_list *line, struct tagged_acx_value *value)
{
    kv_push(struct tagged_acx_value*, *line, value);
    return line;
}

static acx_value_list *make_value_list(struct tagged_acx_value *value)
{
    acx_value_list *values = xmalloc(sizeof(acx_value_list));
    kv_init(*values);
    return push_value(values, value);
}

static int parse_int(const char *tok)
{
    errno = 0;
    char *endptr;
    int i = strtol(tok, &endptr, 0);
    if (errno || *endptr != '\0')
	ERROR("Invalid integer value");
    return i;
}

struct tagged_acx_value *value_int(struct string *s)
{
    struct tagged_acx_value *value = xmalloc(sizeof(struct tagged_acx_value));
    value->type = ACX_INT;
    value->value.i = parse_int(s->text);
    free_string(s);
    return value;
}

struct tagged_acx_value *value_string(struct string *s)
{
    struct tagged_acx_value *value = xmalloc(sizeof(struct tagged_acx_value));
    value->type = ACX_STRING;

    char *u = utf2sjis(s->text, 0);
    value->value.s = make_string(u, strlen(u));
    free(u);
    free_string(s);
    return value;
}

%}

%token	<string>	STRING INTEGER
%token	<token>		NEWLINE COMMA INVALID_TOKEN SYM_INT SYM_STRING

%type	<types>		header types
%type	<type>		type
%type	<lines>		lines
%type	<values>	values line
%type	<value>		value

%start file

%%

file    :	header lines { acx_output = make_acx($1, $2); }
	;

header  :	types NEWLINE { $$ = $1; }
	;

types   :	type             { $$ = make_type_list($1); }
	|	types COMMA type { $$ = push_type($1, $3); }
	;

type    :	SYM_INT    { $$ = ACX_INT; }
	|	SYM_STRING { $$ = ACX_STRING; }
	;

lines   :	line       { $$ = make_record_list($1); }
	|	lines line { $$ = push_record($1, $2); }
	;

line    :	values NEWLINE { $$ = $1; }

values  :	value              { $$ = make_value_list($1); }
	|	values COMMA value { $$ = push_value($1, $3); }
	;

value   :	INTEGER { $$ = value_int($1); }
	|	STRING  { $$ = value_string($1); }
	;

%%
