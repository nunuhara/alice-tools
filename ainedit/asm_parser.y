%define api.prefix {asm_}

%union {
    int token;
    struct string *string;
    parse_argument_list *args;
    struct parse_instruction *instr;
    parse_instruction_list *program;
}

%code requires {
    #include <stdint.h>
    #include "khash.h"
    #include "kvec.h"

    kv_decl(parse_instruction_list, struct parse_instruction*);
    kv_decl(parse_argument_list, struct string*);
    kv_decl(pointer_list, uint32_t*);

    struct parse_instruction {
	uint16_t opcode;
        parse_argument_list *args;
    };

    parse_instruction_list *parsed_code;

    KHASH_MAP_INIT_STR(label_table, uint32_t);
    khash_t(label_table) *label_table;
}

%{

#include <stdio.h>
#include <ctype.h>
#include "ainedit.h"
#include "kvec.h"
#include "system4.h"
#include "system4/instructions.h"
#include "system4/string.h"

extern int asm_lex();
extern unsigned long asm_line;

#define PARSE_ERROR(fmt, ...)						\
    sys_error("ERROR: At line %d: " fmt "\n", asm_line-1, ##__VA_ARGS__)

void asm_error(const char *s)
{
    sys_error("ERROR: At line %d: %s\n", asm_line, s);
}

static uint32_t instr_ptr;

static parse_instruction_list *make_program(void)
{
    parse_instruction_list *program = xmalloc(sizeof(parse_instruction_list));
    kv_init(*program);
    instr_ptr = 0;
    return program;
}

static void push_instruction(parse_instruction_list *program, struct parse_instruction *instr)
{
    kv_push(struct parse_instruction*, *program, instr);
    instr_ptr += asm_instruction_width(instr->opcode);
}

static struct parse_instruction *make_instruction(struct string *name, parse_argument_list *args)
{
    for (int i = 0; name->text[i]; i++) {
        name->text[i] = toupper(name->text[i]);
    }
    // check opcode
    const struct instruction *info = asm_get_instruction(name->text);
    if (!info)
        PARSE_ERROR("Invalid instruction: %s", name->text);
    // check argument count
    size_t nr_args = args ? kv_size(*args) : 0;
    if (nr_args != (size_t)info->nr_args) {
        fprintf(stderr, "In: '%s", info->name);
        for (size_t i = 0; i < nr_args; i++) {
            fprintf(stderr, " %s", kv_A(*args, i)->text);
        }
        fprintf(stderr, "'\n");
        PARSE_ERROR("Wrong number of arguments for instruction '%s' (expected %d; got %lu)",
                    name->text, info->nr_args, nr_args);
    }
    // NOTE: argument values checked on second pass

    free_string(name);

    struct parse_instruction *instr = xmalloc(sizeof (struct parse_instruction));
    instr->opcode = info->opcode;
    instr->args = args;
    return instr;
}

static parse_argument_list *make_arglist(void)
{
    parse_argument_list *args = xmalloc(sizeof(parse_argument_list));
    kv_init(*args);
    return args;
}

static void push_arg(parse_argument_list *args, struct string *arg)
{
    kv_push(struct string*, *args, arg);
}

static void push_label(char *name)
{
    int ret;
    khiter_t k = kh_put(label_table, label_table, name, &ret);
    if (!ret) {
        if (kh_value(label_table, k) != instr_ptr)
            ERROR("Duplicate label: %s", name);
        return;
    }
    kh_value(label_table, k) = instr_ptr;
}

%}

%token	<string>	IDENTIFIER LABEL
%token	<token>		NEWLINE INVALID_TOKEN

%type	<program>	lines
%type	<instr>		line
%type	<args>		args

%start program

%%

program : 	lines { parsed_code = $1; }
	;

lines   :	line { $$ = make_program(); if ($1) { push_instruction($$, $1); } }
	|	lines line { if ($2) { push_instruction($1, $2); } }
	;

line    :	NEWLINE { $$ = NULL; }
	|	LABEL { push_label($1->text); $$ = NULL; }
	|	IDENTIFIER NEWLINE { $$ = make_instruction($1, NULL); }
	|	IDENTIFIER args NEWLINE { $$ = make_instruction($1, $2); }

args    :	IDENTIFIER { $$ = make_arglist(); push_arg($$, $1); }
	|	args IDENTIFIER { push_arg($1, $2); }
	;

%%
