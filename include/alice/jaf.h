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
#include <stddef.h>
#include <stdio.h>
#include "system4.h"
#include "system4/ain.h"
#include "kvec.h"

struct port;

#define _COMPILER_ERROR(file, line, msgf, ...)		\
	ERROR("at %s:%d: " msgf, file ? file : "?", line, ##__VA_ARGS__)

#define COMPILER_ERROR(obj, msgf, ...) \
	_Generic((obj),\
		 struct jaf_expression*: _COMPILER_ERROR((obj)->file, (obj)->line, msgf, ##__VA_ARGS__),\
		 struct jaf_block_item*: _COMPILER_ERROR((obj)->file, (obj)->line, msgf, ##__VA_ARGS__))

#define _JAF_ERROR(file, line, msgf, ...)				\
	jaf_generic_error(file, line, msgf, ##__VA_ARGS__)

#define JAF_ERROR(obj, msgf, ...)					\
	_Generic((obj),							\
		 struct jaf_expression*: jaf_expression_error,		\
		 struct jaf_block_item*: jaf_block_item_error)		\
	(obj, msgf, ##__VA_ARGS__)

enum jaf_type {
	JAF_VOID,
	JAF_INT,
	JAF_BOOL,
	JAF_FLOAT,
	JAF_LONG_INT,
	JAF_STRING,
	JAF_STRUCT,
	JAF_IFACE,
	JAF_ENUM,
	JAF_ARRAY,
	JAF_WRAP,
	JAF_HLL_PARAM,
	JAF_HLL_FUNC_71,
	JAF_HLL_FUNC,
	JAF_IMAIN_SYSTEM,
	JAF_DELEGATE,
	JAF_TYPEDEF,
	JAF_FUNCTYPE
};

enum _jaf_type {
	NR_JAF_TYPES = JAF_FUNCTYPE+1,
	JAF_INTP,
	JAF_FLOATP,
};

/*
 * Intermediate types. These are applied to certain AST nodes but should never
 * be written to the .ain file.
 */
enum _ain_type {
	_AIN_FUNCTION = 255 - 0, // identifier: function
	_AIN_LIBRARY  = 255 - 1, // identifier: HLL library
	_AIN_SYSTEM   = 255 - 2, // identifier: system library
	_AIN_SYSCALL  = 255 - 3, // member: system call reference (e.g. system.Output)
	_AIN_HLLCALL  = 255 - 4, // member: hll call (e.g. Library.Function)
	_AIN_METHOD   = 255 - 5, // member: method reference (e.g. obj.method)
	_AIN_BUILTIN  = 255 - 6, // member: builtin method reference (e.g. "string".Split)
	_AIN_SUPER    = 255 - 7, // super call - can be either function or method call
	_AIN_NULLTYPE = 255 - 8, // untyped NULL expression
	_AIN_IMETHOD  = 255 - 9, // member: interface method reference (e.g. iface.method)
};
#define AIN_FUNCTION ((enum ain_data_type)_AIN_FUNCTION)
#define AIN_LIBRARY  ((enum ain_data_type)_AIN_LIBRARY)
#define AIN_SYSTEM   ((enum ain_data_type)_AIN_SYSTEM)
#define AIN_SYSCALL  ((enum ain_data_type)_AIN_SYSCALL)
#define AIN_HLLCALL  ((enum ain_data_type)_AIN_HLLCALL)
#define AIN_METHOD   ((enum ain_data_type)_AIN_METHOD)
#define AIN_BUILTIN  ((enum ain_data_type)_AIN_BUILTIN)
#define AIN_SUPER    ((enum ain_data_type)_AIN_SUPER)
#define AIN_NULLTYPE ((enum ain_data_type)_AIN_NULLTYPE)
#define AIN_IMETHOD  ((enum ain_data_type)_AIN_IMETHOD)

/*
 * Built-in libraries; these are negative to disambiguate between true built-ins
 * and the special HLL built-ins that replaced them.
 */
enum jaf_builtin_lib {
	JAF_BUILTIN_INT      = -1,
	JAF_BUILTIN_FLOAT    = -2,
	JAF_BUILTIN_STRING   = -3,
	JAF_BUILTIN_ARRAY    = -4,
	JAF_BUILTIN_DELEGATE = -5,
};

enum jaf_builtin_method {
	JAF_INT_STRING,
	JAF_FLOAT_STRING,
	JAF_STRING_INT,
	JAF_STRING_LENGTH,
	JAF_STRING_LENGTHBYTE,
	JAF_STRING_EMPTY,
	JAF_STRING_FIND,
	JAF_STRING_GETPART,
	JAF_STRING_PUSHBACK,
	JAF_STRING_POPBACK,
	JAF_STRING_ERASE,
	JAF_ARRAY_ALLOC,
	JAF_ARRAY_REALLOC,
	JAF_ARRAY_FREE,
	JAF_ARRAY_NUMOF,
	JAF_ARRAY_COPY,
	JAF_ARRAY_FILL,
	JAF_ARRAY_PUSHBACK,
	JAF_ARRAY_POPBACK,
	JAF_ARRAY_EMPTY,
	JAF_ARRAY_ERASE,
	JAF_ARRAY_INSERT,
	JAF_ARRAY_SORT,
	JAF_ARRAY_FIND,
	JAF_DELEGATE_NUMOF,
	JAF_DELEGATE_EXIST,
	JAF_DELEGATE_CLEAR,
};
#define JAF_NR_BUILTINS (JAF_DELEGATE_CLEAR+1)

enum jaf_type_qualifier {
	JAF_QUAL_CONST       = 1,
	JAF_QUAL_REF         = 2,
	//JAF_QUAL_ARRAY       = 4,
	JAF_QUAL_OVERRIDE    = 8,
};

enum jaf_expression_type {
	JAF_EXP_VOID = 0,
	JAF_EXP_INT,
	JAF_EXP_FLOAT,
	JAF_EXP_STRING,
	JAF_EXP_IDENTIFIER,
	JAF_EXP_THIS,
	JAF_EXP_UNARY,
	JAF_EXP_BINARY,
	JAF_EXP_TERNARY,
	JAF_EXP_FUNCALL,
	JAF_EXP_SYSCALL,
	JAF_EXP_HLLCALL,
	JAF_EXP_METHOD_CALL,
	JAF_EXP_INTERFACE_CALL,
	JAF_EXP_BUILTIN_CALL,
	JAF_EXP_SUPER_CALL,
	JAF_EXP_NEW,
	JAF_EXP_CAST,
	JAF_EXP_MEMBER,
	JAF_EXP_SEQ,
	JAF_EXP_SUBSCRIPT,
	JAF_EXP_CHAR,
	JAF_EXP_NULL,
	JAF_EXP_DUMMYREF,
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
	JAF_REQ,
	JAF_RNE,
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
	JAF_CHAR_ASSIGN,
};

struct string;
struct jaf_expression;
struct jaf_block_item;

struct jaf_name {
	size_t nr_parts;
	struct string **parts;
	struct string *collapsed;
	int16_t struct_no;
	bool is_constructor;
	bool is_destructor;
};

struct jaf_argument_list {
	size_t nr_items;
	struct jaf_expression **items;
	int *var_nos;
};

struct jaf_type_specifier {
	enum jaf_type type;
	struct jaf_type_specifier *array_type;
	unsigned qualifiers;
	struct string *name;
	union {
		int struct_no;
		int func_no;
	};
	unsigned rank;
};

enum jaf_ident_type {
	JAF_IDENT_UNRESOLVED,
	JAF_IDENT_LOCAL,
	JAF_IDENT_GLOBAL,
	JAF_IDENT_CONST,
};

enum jaf_dotted_type {
	JAF_DOT_MEMBER,
	JAF_DOT_METHOD,
	JAF_DOT_PROPERTY,
};

struct jaf_expression {
	unsigned line;
	const char *file;
	enum jaf_expression_type type;
	enum jaf_operator op;
	struct ain_type valuetype;
	union {
		int i;
		float f;
		struct string *s;
		struct {
			enum jaf_ident_type kind;
			struct string *name;
			union {
				struct jaf_vardecl *local;
				int global;
				struct ain_initval constval;
			};
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
			int lib_no;
			int type_param;
		} call;
		struct {
			struct jaf_type_specifier *type;
			struct jaf_argument_list *args;
			int func_no;
		} new;
		// struct member
		struct {
			struct jaf_expression *struc;
			struct string *name;
			enum jaf_dotted_type type;
			int object_no;
			int member_no;
			// properties
			int getter_no;
			int setter_no;
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
		// dummy ref ('new' or call with ref return type)
		struct {
			struct jaf_expression *expr;
			int var_no;
		} dummy;
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
	struct jaf_name name;
	struct jaf_block *params;
};

enum block_item_kind {
	JAF_DECL_VAR,
	JAF_DECL_FUN,
	JAF_DECL_FUNCTYPE,
	JAF_DECL_DELEGATE,
	JAF_DECL_STRUCT,
	JAF_DECL_INTERFACE,
	//JAF_DECL_ENUM,
	JAF_STMT_NULL,
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
	JAF_STMT_RASSIGN,
	JAF_STMT_ASSERT,
	JAF_EOF
};

struct jaf_vardecl {
	struct string *name;
	struct jaf_type_specifier *type;
	struct ain_type valuetype;
	struct jaf_expression **array_dims;
	struct jaf_expression *init;
	enum ain_variable_type var_type;
	int var;
};

typedef kvec_t(struct jaf_vardecl*) jaf_var_set;

enum jaf_fundecl_type {
	JAF_FUN_PROCEDURE,
	JAF_FUN_METHOD,
	JAF_FUN_CONSTRUCTOR,
	JAF_FUN_DESTRUCTOR,
};

struct jaf_fundecl {
	struct jaf_name name;
	struct jaf_type_specifier *type;
	struct ain_type valuetype;
	struct jaf_block *params;
	struct jaf_block *body;
	enum jaf_fundecl_type fun_type;
	int func_no;
	int super_no;
};

typedef kvec_t(struct string*) jaf_string_list;

// declaration or statement
struct jaf_block_item {
	unsigned line;
	const char *file;
	bool is_scope;
	jaf_var_set delete_vars;
	enum block_item_kind kind;
	union {
		struct jaf_vardecl var;
		struct jaf_fundecl fun;
		struct {
			struct string *name;
			struct jaf_block *members;
			struct jaf_block *methods;
			jaf_string_list interfaces;
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
		struct {
			struct jaf_expression *lhs;
			struct jaf_expression *rhs;
		} rassign;
		struct {
			struct jaf_expression *expr;
			struct jaf_expression *expr_string;
			int line;
			struct jaf_expression *file;
		} assertion;
		struct string *target; // goto
		unsigned file_no;      // eof
	};
};

struct jaf_block {
	size_t nr_items;
	struct jaf_block_item **items;
};

struct jaf_env_local {
	bool is_const;
	char *name;
	union {
		// not const: variable
		struct jaf_vardecl *decl;
		// const: (constant) expression
		struct ain_initval val;
	};
};

struct jaf_env {
	struct ain *ain;
	struct jaf_env *parent;
	int func_no;
	struct jaf_fundecl *fundecl;
	size_t nr_locals;
	struct jaf_env_local *locals;
};

_Noreturn void jaf_generic_error(const char *file, int line, const char *msgf, ...);
_Noreturn void jaf_expression_error(struct jaf_expression *expr, const char *msgf, ...);
_Noreturn void jaf_block_item_error(struct jaf_block_item *item, const char *msgf, ...);

// jaf_ast.c
struct jaf_expression *jaf_expr(enum jaf_expression_type type, enum jaf_operator op);
struct jaf_expression *jaf_null(void);
struct jaf_expression *jaf_integer(int i);
struct jaf_expression *jaf_parse_integer(struct string *text);
struct jaf_expression *jaf_float(float f);
struct jaf_expression *jaf_parse_float(struct string *text);
struct string *jaf_process_string(struct string *text);
struct jaf_expression *jaf_string(struct string *text);
struct jaf_expression *jaf_char(struct string *text);
void jaf_name_init(struct jaf_name *name, struct string *str);
void jaf_name_append(struct jaf_name *name, struct string *str);
void jaf_name_prepend(struct jaf_name *name, struct string *str);
struct jaf_expression *jaf_identifier(struct string *name);
struct jaf_expression *jaf_this(void);
struct string *jaf_method_name(struct string *ns, struct string *name);
struct jaf_expression *jaf_unary_expr(enum jaf_operator op, struct jaf_expression *expr);
struct jaf_expression *jaf_binary_expr(enum jaf_operator op, struct jaf_expression *lhs, struct jaf_expression *rhs);
struct jaf_expression *jaf_ternary_expr(struct jaf_expression *test, struct jaf_expression *cons, struct jaf_expression *alt);
struct jaf_expression *jaf_seq_expr(struct jaf_expression *head, struct jaf_expression *tail);
struct jaf_expression *jaf_function_call(struct jaf_expression *fun, struct jaf_argument_list *args);
struct jaf_expression *jaf_system_call(struct string *name, struct jaf_argument_list *args);
struct jaf_expression *jaf_new(struct jaf_type_specifier *type, struct jaf_argument_list *args);
struct jaf_expression *jaf_cast_expression(enum jaf_type type, struct jaf_expression *expr);
struct jaf_expression *jaf_member_expr(struct jaf_expression *struc, struct string *name);
struct jaf_expression *jaf_subscript_expr(struct jaf_expression *expr, struct jaf_expression *index);
struct jaf_argument_list *jaf_args(struct jaf_argument_list *head, struct jaf_expression *tail);
struct jaf_type_specifier *jaf_type(enum jaf_type type);
struct jaf_type_specifier *jaf_typedef(struct string *name);
struct jaf_type_specifier *jaf_array_type(struct jaf_type_specifier *type, int rank);
struct jaf_type_specifier *jaf_wrap(struct jaf_type_specifier *type);
struct jaf_declarator *jaf_declarator(struct string *name);
struct jaf_declarator *jaf_array_allocation(struct string *name, struct jaf_expression *dim);
struct jaf_declarator *jaf_array_dimension(struct jaf_declarator *d, struct jaf_expression *dim);
struct jaf_declarator_list *jaf_declarators(struct jaf_declarator_list *head, struct jaf_declarator *tail);
struct jaf_function_declarator *jaf_function_declarator(struct jaf_name *name, struct jaf_block *params);
struct jaf_function_declarator *jaf_function_declarator_simple(struct string *str,
		struct jaf_block *params);
struct jaf_block *jaf_parameter(struct jaf_type_specifier *type, struct jaf_declarator *declarator);
struct jaf_block_item *_jaf_function(struct jaf_type_specifier *type, struct jaf_name *name,
		struct jaf_block *params, struct jaf_block *body);
struct jaf_block *jaf_function(struct jaf_type_specifier *type,
		struct jaf_function_declarator *decl, struct jaf_block *body);
struct jaf_block *jaf_constructor(struct string *name, struct jaf_block *body);
struct jaf_block *jaf_destructor(struct string *name, struct jaf_block *body);
struct jaf_block *jaf_vardecl(struct jaf_type_specifier *type, struct jaf_declarator_list *declarators);
struct jaf_block *jaf_merge_blocks(struct jaf_block *head, struct jaf_block *tail);
struct jaf_block *jaf_block(struct jaf_block_item *item);
struct jaf_block *jaf_block_append(struct jaf_block *head, struct jaf_block_item *tail);
struct jaf_block_item *jaf_null_statement(void);
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
struct jaf_block_item *jaf_struct(struct string *name, struct jaf_block *fields,
		jaf_string_list *interfaces);
struct jaf_block_item *jaf_interface(struct string *name, struct jaf_block *methods);
struct jaf_block_item *jaf_rassign(struct jaf_expression *lhs, struct jaf_expression *rhs);
struct jaf_block_item *jaf_assert(struct jaf_expression *expr, int line, const char *file);
struct jaf_expression *jaf_copy_expression(struct jaf_expression *e);
void jaf_free_expr(struct jaf_expression *expr);
void jaf_free_block(struct jaf_block *block);

// jaf_parser.y
extern struct ain *jaf_ain_out;
extern struct jaf_block *jaf_toplevel;
struct jaf_block *jaf_parse(struct ain *ain, const char **files, unsigned nr_files);

// jaf_compile.c
void jaf_build(struct ain *out, const char **files, unsigned nr_files, const char **headers, unsigned nr_headers);

// jaf_eval.c
struct jaf_expression *jaf_simplify(struct jaf_expression *in);

// jaf_types.c
void jaf_type_check_expression(struct jaf_env *env, struct jaf_expression *expr);
void jaf_type_check_statement(struct jaf_env *env, struct jaf_block_item *stmt);
void jaf_type_check_vardecl(struct jaf_env *env, struct jaf_block_item *decl);

// jaf_static_analysis.c
struct jaf_block *jaf_static_analyze(struct ain *ain, struct jaf_block *block);
struct jaf_env *jaf_env_push(struct jaf_env *parent);
struct jaf_env *jaf_env_pop(struct jaf_env *env);
struct jaf_env *jaf_env_push_function(struct jaf_env *parent, struct jaf_fundecl *decl);
void jaf_env_add_local(struct jaf_env *env, struct jaf_vardecl *decl);
struct jaf_env_local *jaf_env_lookup(struct jaf_env *env, const char *name);

// jaf_resolve.c
void jaf_resolve_types(struct ain *ain, struct jaf_block *block);

// jaf_declaration.c
void jaf_process_declarations(struct ain *ain, struct jaf_block *block);
void jaf_process_hll_declarations(struct ain *ain, struct jaf_block *block, const char *hll_name);
void jaf_allocate_variables(struct ain *ain, struct jaf_block *block);
struct string *jaf_name_collapse(struct ain *ain, struct jaf_name *name);

// jaf_visitor.c
struct jaf_visitor {
	void(*visit_stmt_pre)(struct jaf_block_item*,struct jaf_visitor*);
	void(*visit_stmt_post)(struct jaf_block_item*,struct jaf_visitor*);
	struct jaf_expression*(*visit_expr_pre)(struct jaf_expression*,struct jaf_visitor*);
	struct jaf_expression*(*visit_expr_post)(struct jaf_expression*, struct jaf_visitor*);
	struct jaf_block_item *stmt;
	struct jaf_expression *expr;
	struct jaf_env *env;
	void *data;
};
void jaf_accept_block(struct ain *ain, struct jaf_block *block, struct jaf_visitor *visitor);

// jaf_ain.c
void jaf_define_struct(struct ain *ain, struct jaf_block_item *type);
void jaf_define_interface(struct ain *ain, struct jaf_block_item *def);
void jaf_define_functype(struct ain *ain, struct jaf_block_item *item);
void jaf_define_delegate(struct ain *ain, struct jaf_block_item *item);
void jaf_to_ain_type(struct ain *ain, struct ain_type *out, struct jaf_type_specifier *in);
enum ain_data_type jaf_to_ain_simple_type(enum jaf_type type);

// jaf_error.c
const char *jaf_type_to_string(enum jaf_type type);
void jaf_print_expression(struct port *out, struct jaf_expression *expr);
struct string *jaf_expression_to_string(struct jaf_expression *expr);

#endif /* AINEDIT_JAF_H */
