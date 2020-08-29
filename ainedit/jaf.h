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

#ifndef AINEDIT_JAF_H
#define AINEDIT_JAF_H

#include <stdbool.h>
#include <stdio.h>
#include "system4/ain.h"

struct string;

enum jaf_type {
	JAF_VOID,
	JAF_INT,
	JAF_FLOAT,
	JAF_STRING,
	JAF_STRUCT,
	JAF_ENUM,
	JAF_TYPEDEF,
	JAF_FUNCTYPE,
};

enum _jaf_type {
	NR_JAF_TYPES = JAF_FUNCTYPE+1,
	JAF_INTP,
	JAF_FLOATP,
};

enum jaf_type_qualifier {
	JAF_QUAL_CONST       = 1,
	JAF_QUAL_REF         = 2,
	JAF_QUAL_ARRAY       = 4,
	JAF_QUAL_CONSTRUCTOR = 8,
	JAF_QUAL_DESTRUCTOR  = 16,
};

enum jaf_expression_type {
	JAF_EXP_VOID = 0,
	JAF_EXP_INT,
	JAF_EXP_FLOAT,
	JAF_EXP_STRING,
	JAF_EXP_IDENTIFIER,
	JAF_EXP_UNARY,
	JAF_EXP_BINARY,
	JAF_EXP_TERNARY,
	JAF_EXP_FUNCALL,
	JAF_EXP_SYSCALL,
	//JAF_EXP_HLLCALL,
	JAF_EXP_CAST,
	JAF_EXP_MEMBER,
	JAF_EXP_SEQ,
	JAF_EXP_SUBSCRIPT,
	JAF_EXP_CHAR,
};

enum jaf_operator {
	JAF_NO_OPERATOR = 0,
	JAF_AMPERSAND,
	JAF_UNARY_PLUS,
	JAF_UNARY_MINUS,
	JAF_BIT_NOT,
	JAF_LOG_NOT,
	JAF_PRE_INC,
	JAF_PRE_DEC,
	JAF_POST_INC,
	JAF_POST_DEC,

	JAF_MULTIPLY,
	JAF_DIVIDE,
	JAF_REMAINDER,
	JAF_PLUS,
	JAF_MINUS,
	JAF_LSHIFT,
	JAF_RSHIFT,
	JAF_LT,
	JAF_GT,
	JAF_LTE,
	JAF_GTE,
	JAF_EQ,
	JAF_NEQ,
	JAF_BIT_AND,
	JAF_BIT_XOR,
	JAF_BIT_IOR,
	JAF_LOG_AND,
	JAF_LOG_OR,
	JAF_ASSIGN,
	JAF_MUL_ASSIGN,
	JAF_DIV_ASSIGN,
	JAF_MOD_ASSIGN,
	JAF_ADD_ASSIGN,
	JAF_SUB_ASSIGN,
	JAF_LSHIFT_ASSIGN,
	JAF_RSHIFT_ASSIGN,
	JAF_AND_ASSIGN,
	JAF_XOR_ASSIGN,
	JAF_OR_ASSIGN,
	JAF_REF_ASSIGN,
};

struct jaf_argument_list {
	size_t nr_items;
	struct jaf_expression **items;
	int *var_nos;
};

struct jaf_type_specifier {
	enum jaf_type type;
	unsigned qualifiers;
	struct string *name;
	union {
		int struct_no;
		int func_no;
	};
	unsigned rank;
};

struct jaf_expression {
	enum jaf_expression_type type;
	enum jaf_operator op;
	struct ain_type valuetype;
	union {
		int i;
		float f;
		struct string *s;
		struct {
			struct string *name;
			enum ain_variable_type var_type;
			int var_no;
		} ident;
		// unary operators
		struct jaf_expression *expr;
		// binary operators
		struct {
			struct jaf_expression *lhs;
			struct jaf_expression *rhs;
		};
		// ternary operator
		struct {
			struct jaf_expression *condition;
			struct jaf_expression *consequent;
			struct jaf_expression *alternative;
		};
		// cast
		struct {
			enum jaf_type type;
			struct jaf_expression *expr;
		} cast;
		// function call
		struct {
			struct jaf_expression *fun;
			struct jaf_argument_list *args;
			int func_no;
		} call;
		// struct member
		struct {
			struct jaf_expression *struc;
			struct string *name;
			int member_no;
		} member;
		// sequence
		struct {
			struct jaf_expression *head;
			struct jaf_expression *tail;
		} seq;
		// subscript
		struct {
			struct jaf_expression *expr;
			struct jaf_expression *index;
		} subscript;
	};
};

struct jaf_declarator {
	struct string *name;
	struct jaf_expression *init;
	size_t array_rank;
	struct jaf_expression **array_dims;
};

struct jaf_declarator_list {
	size_t nr_decls;
	struct jaf_declarator **decls;
};

struct jaf_function_declarator {
	struct string *name;
	struct jaf_block *params;
};

enum block_item_kind {
	JAF_DECL_VAR,
	JAF_DECL_FUN,
	JAF_DECL_FUNCTYPE,
	JAF_DECL_STRUCT,
	//JAF_DECL_ENUM,
	JAF_STMT_LABELED,
	JAF_STMT_COMPOUND,
	JAF_STMT_EXPRESSION,
	JAF_STMT_IF,
	JAF_STMT_SWITCH,
	JAF_STMT_WHILE,
	JAF_STMT_DO_WHILE,
	JAF_STMT_FOR,
	JAF_STMT_GOTO,
	JAF_STMT_CONTINUE,
	JAF_STMT_BREAK,
	JAF_STMT_RETURN,
	JAF_STMT_CASE,
	JAF_STMT_DEFAULT,
	JAF_STMT_MESSAGE,
	JAF_EOF
};

struct jaf_vardecl {
	struct string *name;
	struct jaf_type_specifier *type;
	struct ain_type valuetype;
	struct jaf_expression **array_dims;
	struct jaf_expression *init;
	int var_no;
};

struct jaf_fundecl {
	struct string *name;
	struct jaf_type_specifier *type;
	struct ain_type valuetype;
	struct jaf_block *params;
	struct jaf_block *body;
	int func_no;
};

// declaration or statement
struct jaf_block_item {
	enum block_item_kind kind;
	union {
		struct jaf_vardecl var;
		struct jaf_fundecl fun;
		struct {
			struct string *name;
			struct jaf_block *members;
			struct jaf_block *methods;
			int struct_no;
		} struc;
		struct {
			struct string *name;
			struct jaf_block_item *stmt;
		} label;
		struct {
			struct jaf_expression *expr;
			struct jaf_block_item *stmt;
		} swi_case;
		struct jaf_block *block;
		struct jaf_expression *expr;
		struct {
			struct jaf_expression *test;
			struct jaf_block_item *consequent;
			struct jaf_block_item *alternative;
		} cond;
		struct {
			struct jaf_expression *expr;
			struct jaf_block *body;
		} swi;
		struct {
			struct jaf_expression *test;
			struct jaf_block_item *body;
		} while_loop;
		struct {
			struct jaf_block *init;
			struct jaf_expression *test;
			struct jaf_expression *after;
			struct jaf_block_item *body;
		} for_loop;
		struct {
			struct string *text;
			struct string *func;
			int func_no;
		} msg;
		struct string *target; // goto
		unsigned file_no;      // eof
	};
};

struct jaf_block {
	size_t nr_items;
	struct jaf_block_item **items;
};

struct jaf_env {
	struct ain *ain;
	struct jaf_env *parent;
	int func_no;
	struct jaf_fundecl *fundecl;
	size_t nr_locals;
	struct ain_variable **locals;
};

struct jaf_expression *jaf_integer(int i);
struct jaf_expression *jaf_parse_integer(struct string *text);
struct jaf_expression *jaf_float(float f);
struct jaf_expression *jaf_parse_float(struct string *text);
struct string *jaf_process_string(struct string *text);
struct jaf_expression *jaf_string(struct string *text);
struct jaf_expression *jaf_char(struct string *text);
struct jaf_expression *jaf_identifier(struct string *name);
struct jaf_expression *jaf_unary_expr(enum jaf_operator op, struct jaf_expression *expr);
struct jaf_expression *jaf_binary_expr(enum jaf_operator op, struct jaf_expression *lhs, struct jaf_expression *rhs);
struct jaf_expression *jaf_ternary_expr(struct jaf_expression *test, struct jaf_expression *cons, struct jaf_expression *alt);
struct jaf_expression *jaf_seq_expr(struct jaf_expression *head, struct jaf_expression *tail);
struct jaf_expression *jaf_function_call(struct jaf_expression *fun, struct jaf_argument_list *args);
struct jaf_expression *jaf_system_call(struct string *name, struct jaf_argument_list *args);
struct jaf_expression *jaf_cast_expression(enum jaf_type type, struct jaf_expression *expr);
struct jaf_expression *jaf_member_expr(struct jaf_expression *struc, struct string *name);
struct jaf_expression *jaf_subscript_expr(struct jaf_expression *expr, struct jaf_expression *index);

struct jaf_argument_list *jaf_args(struct jaf_argument_list *head, struct jaf_expression *tail);

struct jaf_type_specifier *jaf_type(enum jaf_type type);
struct jaf_type_specifier *jaf_typedef(struct string *name);
struct jaf_type_specifier *jaf_array_type(struct jaf_type_specifier *type, int rank);

struct jaf_declarator *jaf_declarator(struct string *name);
struct jaf_declarator *jaf_array_allocation(struct string *name, struct jaf_expression *dim);
struct jaf_declarator *jaf_array_dimension(struct jaf_declarator *d, struct jaf_expression *dim);
struct jaf_declarator_list *jaf_declarators(struct jaf_declarator_list *head, struct jaf_declarator *tail);

struct jaf_block *jaf_parameter(struct jaf_type_specifier *type, struct jaf_declarator *declarator);
struct jaf_function_declarator *jaf_function_declarator(struct string *name, struct jaf_block *params);
struct jaf_block *jaf_function(struct jaf_type_specifier *type, struct jaf_function_declarator *decl, struct jaf_block *body);
struct jaf_block *jaf_constructor(struct string *name, struct jaf_block *body);
struct jaf_block *jaf_destructor(struct string *name, struct jaf_block *body);

struct jaf_block *jaf_vardecl(struct jaf_type_specifier *type, struct jaf_declarator_list *declarators);
struct jaf_block *jaf_merge_blocks(struct jaf_block *head, struct jaf_block *tail);

struct jaf_block *jaf_block(struct jaf_block_item *item);
struct jaf_block *jaf_block_append(struct jaf_block *head, struct jaf_block_item *tail);
struct jaf_block_item *jaf_compound_statement(struct jaf_block *block);
struct jaf_block_item *jaf_label_statement(struct string *label, struct jaf_block_item *stmt);
struct jaf_block_item *jaf_case_statement(struct jaf_expression *expr, struct jaf_block_item *stmt);
struct jaf_block_item *jaf_expression_statement(struct jaf_expression *expr);
struct jaf_block_item *jaf_if_statement(struct jaf_expression *test, struct jaf_block_item *cons, struct jaf_block_item *alt);
struct jaf_block_item *jaf_switch_statement(struct jaf_expression *expr, struct jaf_block *body);
struct jaf_block_item *jaf_while_loop(struct jaf_expression *test, struct jaf_block_item *body);
struct jaf_block_item *jaf_do_while_loop(struct jaf_expression *test, struct jaf_block_item *body);
struct jaf_block_item *jaf_for_loop(struct jaf_block *init, struct jaf_block_item *test, struct jaf_expression *after, struct jaf_block_item *body);
struct jaf_block_item *jaf_goto(struct string *target);
struct jaf_block_item *jaf_continue(void);
struct jaf_block_item *jaf_break(void);
struct jaf_block_item *jaf_return(struct jaf_expression *expr);
struct jaf_block_item *jaf_message_statement(struct string *msg, struct string *func);
struct jaf_block_item *jaf_struct(struct string *name, struct jaf_block *fields);

void jaf_free_expr(struct jaf_expression *expr);
void jaf_free_block(struct jaf_block *block);

// jaf_parser.y
struct ain *jaf_ain_out;
struct jaf_block *jaf_toplevel;
struct jaf_block *jaf_parse(struct ain *ain, const char **files, unsigned nr_files);

// jaf_compile.c
void jaf_build(struct ain *out, const char **files, unsigned nr_files, const char **headers, unsigned nr_headers);
// jaf_eval.c
struct jaf_expression *jaf_simplify(struct jaf_expression *in);

// jaf_types.c
const char *jaf_typestr(enum jaf_type type);
void jaf_derive_types(struct jaf_env *env, struct jaf_expression *expr);
void jaf_check_type(struct jaf_expression *expr, struct ain_type *type);

// jaf_static_analysis.c
void jaf_resolve_declarations(struct ain *ain, struct jaf_block *block);
void jaf_resolve_hll_declarations(struct ain *ain, struct jaf_block *block, const char *hll_name);
struct jaf_block *jaf_static_analyze(struct ain *ain, struct jaf_block *block);
enum ain_data_type jaf_to_ain_data_type(enum jaf_type type, unsigned qualifiers);
void jaf_define_struct(struct ain *ain, struct jaf_block_item *type);
void jaf_define_functype(struct ain *ain, struct jaf_fundecl *decl);

#endif /* AINEDIT_JAF_H */
