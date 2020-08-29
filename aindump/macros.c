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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "aindump.h"
#include "dasm.h"

// Returns true if the current instruction can be elided.
// We need to prevent eliding instructions that are jump targets.
static bool can_elide(struct dasm_state *dasm)
{
	return !dasm_eof(dasm) && !dasm_is_jump_target(dasm);
}

static void print_local(struct dasm_state *dasm, int32_t no)
{
	dasm_print_local_variable(dasm, &dasm->ain->functions[dasm->func], no);
}

static bool is_local(struct dasm_state *dasm, int32_t no)
{
	return no >= 0 && no < dasm->ain->functions[dasm->func].nr_vars;
}

static bool is_global(struct dasm_state *dasm, int32_t no)
{
	return no >= 0 && no < dasm->ain->nr_globals;
}

static bool is_struct_type(struct dasm_state *dasm, int32_t no)
{
	return no >= 0 && no < dasm->ain->nr_structures;
}

static bool is_member(struct dasm_state *dasm, int32_t no)
{
	int struct_type = dasm->ain->functions[dasm->func].struct_type;
	if (struct_type < 0)
		return false;
	if (no < 0 || no >= dasm->ain->structures[struct_type].nr_members)
		return false;
	return true;
}

static bool local_check(struct dasm_state *dasm, int32_t *args)
{
	return is_local(dasm, args[0]);
}
#define LOCALREF_check      local_check
#define LOCALREFREF_check   local_check
#define LOCALINC_check      local_check
#define LOCALINC2_check     local_check
#define LOCALDEC_check      local_check
#define LOCALDEC2_check     local_check
#define LOCALPLUSA_check    local_check
#define LOCALMINUSA_check   local_check
#define LOCALASSIGN_check   local_check
#define LOCALASSIGN2_check  local_check
#define F_LOCALASSIGN_check local_check
#define S_LOCALASSIGN_check local_check

static bool STACK_LOCALASSIGN_check(struct dasm_state *dasm, int32_t *args)
{
	return is_local(dasm, args[0]) && args[1] == args[0];
}

static bool LOCALCREATE_check(struct dasm_state *dasm, int32_t *args)
{
	return is_local(dasm, args[0]) && is_struct_type(dasm, args[1]) && args[2] == -1;
}

static bool LOCALDELETE_check(struct dasm_state *dasm, int32_t *args)
{
	return is_local(dasm, args[0]) && args[1] == -1;
}

static void local_emit(struct dasm_state *dasm, int32_t *args)
{
	print_local(dasm, args[0]);
}
#define LOCALREF_emit          local_emit
#define LOCALREFREF_emit       local_emit
#define LOCALINC_emit          local_emit
#define LOCALINC2_emit         local_emit
#define LOCALDEC_emit          local_emit
#define LOCALDEC2_emit         local_emit
#define LOCALDELETE_emit       local_emit
#define LOCALASSIGN2_emit      local_emit
#define STACK_LOCALASSIGN_emit local_emit

static void localassign_emit(struct dasm_state *dasm, int32_t *args)
{
	print_local(dasm, args[0]);
	fprintf(dasm->out, " %d", args[1]);
}
#define LOCALASSIGN_emit localassign_emit
#define LOCALPLUSA_emit  localassign_emit
#define LOCALMINUSA_emit localassign_emit

static void F_LOCALASSIGN_emit(struct dasm_state *dasm, int32_t *args)
{
	union { int32_t i; float f; } v = { .i = args[1] };
	print_local(dasm, args[0]);
	fprintf(dasm->out, " %f", v.f);
}

static void S_LOCALASSIGN_emit(struct dasm_state *dasm, int32_t *args)
{
	print_local(dasm, args[0]);
	fputc(' ', dasm->out);
	dasm_print_string(dasm, dasm->ain->strings[args[1]]->text);
}

static void LOCALCREATE_emit(struct dasm_state *dasm, int32_t *args)
{
	print_local(dasm, args[0]);
	fputc(' ', dasm->out);
	dasm_print_identifier(dasm, dasm->ain->structures[args[1]].name);
}

static bool global_check(struct dasm_state *dasm, int32_t *args)
{
	return is_global(dasm, args[0]);
}
#define GLOBALREF_check      global_check
#define GLOBALREFREF_check   global_check
#define GLOBALINC_check      global_check
#define GLOBALDEC_check      global_check
#define GLOBALASSIGN_check   global_check
#define F_GLOBALASSIGN_check global_check

static void global_emit(struct dasm_state *dasm, int32_t *args)
{
	dasm_print_identifier(dasm, dasm->ain->globals[args[0]].name);
}
#define GLOBALREF_emit    global_emit
#define GLOBALREFREF_emit global_emit
#define GLOBALINC_emit    global_emit
#define GLOBALDEC_emit    global_emit

static void GLOBALASSIGN_emit(struct dasm_state *dasm, int32_t *args)
{
	dasm_print_identifier(dasm, dasm->ain->globals[args[0]].name);
	fprintf(dasm->out, " %d", args[1]);
}

static void F_GLOBALASSIGN_emit(struct dasm_state *dasm, int32_t *args)
{
	union { int32_t i; float f; } v = { .i = args[1] };
	dasm_print_identifier(dasm, dasm->ain->globals[args[0]].name);
	fprintf(dasm->out, " %f", v.f);
}

static bool struct_check(struct dasm_state *dasm, int32_t *args)
{
	return is_member(dasm, args[0]);
}
#define STRUCTREF_check      struct_check
#define STRUCTREFREF_check   struct_check
#define STRUCTINC_check      struct_check
#define STRUCTDEC_check      struct_check
#define STRUCTASSIGN_check   struct_check
#define F_STRUCTASSIGN_check struct_check

static void struct_emit(struct dasm_state *dasm, int32_t *args)
{
	struct ain_struct *s = &dasm->ain->structures[dasm->ain->functions[dasm->func].struct_type];
	dasm_print_identifier(dasm, s->name);
	fputc(' ', dasm->out);
	dasm_print_identifier(dasm, s->members[args[0]].name);
}
#define STRUCTREF_emit    struct_emit
#define STRUCTREFREF_emit struct_emit
#define STRUCTINC_emit    struct_emit
#define STRUCTDEC_emit    struct_emit

static void STRUCTASSIGN_emit(struct dasm_state *dasm, int32_t *args)
{
	struct_emit(dasm, args);
	fprintf(dasm->out, " %d", args[1]);
}

static void F_STRUCTASSIGN_emit(struct dasm_state *dasm, int32_t *args)
{
	union { int32_t i; float f; } v = { .i = args[1] };
	struct_emit(dasm, args);
	fprintf(dasm->out, " %f", v.f);
}

static bool PUSHVMETHOD_check(struct dasm_state *dasm, int32_t *args)
{
	if (args[0] < 0 || !is_member(dasm, args[1]))
		return false;
	struct ain_struct *s = &dasm->ain->structures[dasm->ain->functions[dasm->func].struct_type];
	if (strcmp("<vtable>", s->members[args[1]].name))
		return false;
	return args[2] >= 0;
}

static void PUSHVMETHOD_emit(struct dasm_state *dasm, int32_t *args)
{
	fprintf(dasm->out, "%d %d", args[0], args[2]);
}

#define MACRO_INSTRUCTIONS_MAX 64
#define MACRO_CHILDREN_MAX 16

#define DEFMACRO(_name, ...) {					\
		.name = "." #_name,				\
		.instructions = { __VA_ARGS__, NR_OPCODES },	\
		.check = _name ## _check,			\
		.emit = _name ## _emit,				\
	}

struct macrodef {
	const char * const name;
	enum opcode instructions[MACRO_INSTRUCTIONS_MAX];
	bool (*check)(struct dasm_state *dasm, int32_t *args);
	void (*emit)(struct dasm_state *dasm, int32_t *args);
};

struct macrodef macrodefs[] = {
	DEFMACRO(LOCALREF,          PUSHLOCALPAGE,  PUSH, REF),
	DEFMACRO(LOCALREFREF,       PUSHLOCALPAGE,  PUSH, REFREF),
	DEFMACRO(LOCALINC,          PUSHLOCALPAGE,  PUSH, INC),
	DEFMACRO(LOCALINC2,         PUSHLOCALPAGE,  PUSH, DUP2, REF, DUP_X2, POP, INC, POP),
	DEFMACRO(LOCALDEC,          PUSHLOCALPAGE,  PUSH, DEC),
	DEFMACRO(LOCALDEC2,         PUSHLOCALPAGE,  PUSH, DUP2, REF, DUP_X2, POP, DEC, POP),
	DEFMACRO(LOCALPLUSA,        PUSHLOCALPAGE,  PUSH, PUSH, PLUSA, POP),
	DEFMACRO(LOCALMINUSA,       PUSHLOCALPAGE,  PUSH, PUSH, MINUSA, POP),
	DEFMACRO(LOCALASSIGN,       PUSHLOCALPAGE,  PUSH, PUSH, ASSIGN, POP),
	DEFMACRO(LOCALASSIGN2,      PUSHLOCALPAGE,  SWAP, PUSH, SWAP, ASSIGN),
	DEFMACRO(STACK_LOCALASSIGN, PUSHLOCALPAGE,  PUSH, REF, DELETE, PUSHLOCALPAGE, SWAP, PUSH, SWAP, ASSIGN),
	DEFMACRO(F_LOCALASSIGN,     PUSHLOCALPAGE,  PUSH, F_PUSH, F_ASSIGN, POP),
	DEFMACRO(S_LOCALASSIGN, PUSHLOCALPAGE, PUSH, REF, S_PUSH, S_ASSIGN, DELETE),
	DEFMACRO(LOCALCREATE,       PUSHLOCALPAGE,  PUSH, DUP2, REF, DELETE, DUP2, NEW, ASSIGN, POP, POP, POP),
	DEFMACRO(LOCALDELETE,       PUSHLOCALPAGE,  PUSH, DUP2, REF, DELETE, PUSH, ASSIGN, POP),
	DEFMACRO(GLOBALREF,         PUSHGLOBALPAGE, PUSH, REF),
	DEFMACRO(GLOBALREFREF,      PUSHGLOBALPAGE, PUSH, REFREF),
	DEFMACRO(GLOBALINC,         PUSHGLOBALPAGE, PUSH, INC),
	DEFMACRO(GLOBALDEC,         PUSHGLOBALPAGE, PUSH, DEC),
	DEFMACRO(GLOBALASSIGN,      PUSHGLOBALPAGE, PUSH, PUSH, ASSIGN, POP),
	DEFMACRO(F_GLOBALASSIGN,    PUSHGLOBALPAGE, PUSH, F_PUSH, F_ASSIGN, POP),
	DEFMACRO(STRUCTREF,         PUSHSTRUCTPAGE, PUSH, REF),
	DEFMACRO(STRUCTREFREF,      PUSHSTRUCTPAGE, PUSH, REFREF),
	DEFMACRO(STRUCTINC,         PUSHSTRUCTPAGE, PUSH, INC),
	DEFMACRO(STRUCTDEC,         PUSHSTRUCTPAGE, PUSH, DEC),
	DEFMACRO(STRUCTASSIGN,      PUSHSTRUCTPAGE, PUSH, PUSH, ASSIGN, POP),
	DEFMACRO(F_STRUCTASSIGN,    PUSHSTRUCTPAGE, PUSH, F_PUSH, F_ASSIGN, POP),
	DEFMACRO(PUSHVMETHOD,       PUSHSTRUCTPAGE, PUSH, DUP_U2, PUSH, REF, SWAP, PUSH, ADD, REF),
};

struct macro_node {
	const char * name;
	enum opcode opcode;
	int nr_children;
	struct macro_node *children;
	struct macro_node *parent;
	dasm_save_t save;
	bool (*check)(struct dasm_state *state, int32_t *args);
	void (*emit)(struct dasm_state *state, int32_t *args);
};

static struct macro_node *macros;

static struct macro_node *find_node(struct macro_node *parent, enum opcode opcode)
{
	for (int i = 0; i < parent->nr_children; i++) {
		if (parent->children[i].opcode == opcode)
			return &parent->children[i];
	}
	return NULL;
}

static struct macro_node *add_child(struct macro_node *parent, enum opcode opcode)
{
	int i = parent->nr_children;
	if (i >= MACRO_CHILDREN_MAX)
		ERROR("Exceeded macro branch limit");
	parent->children[i] = (struct macro_node) {
		.opcode = opcode,
		.nr_children = 0,
		.children = xcalloc(MACRO_CHILDREN_MAX, sizeof(struct macro_node)),
		.parent = parent,
		.check = NULL,
		.emit = NULL,
	};
	parent->nr_children++;
	return &parent->children[i];
}

static void add_macro(struct macrodef *macro)
{
	struct macro_node *parent = macros;
	for (int i = 0; macro->instructions[i] < NR_OPCODES; i++) {
		struct macro_node *child = find_node(parent, macro->instructions[i]);
		if (!child)
			child = add_child(parent, macro->instructions[i]);
		parent = child;
	}

	if (parent->emit) {
		WARNING("Conflicting macro definitions (when adding %s)", macro->name);
		return;
	}

	parent->name = macro->name;
	parent->check = macro->check;
	parent->emit = macro->emit;
}

// Transform macro list into a tree.
static void create_macro_tree(void)
{
	macros = xcalloc(1, sizeof(struct macro_node));
	macros->children = xcalloc(MACRO_CHILDREN_MAX, sizeof(struct macro_node));
	for (size_t i = 0; i < sizeof(macrodefs)/sizeof(*macrodefs); i++) {
		add_macro(&macrodefs[i]);
	}
}

static struct macro_node *match_rewind(struct dasm_state *dasm, struct macro_node *match)
{
	do {
		match = match->parent;
	} while (match && !match->check);

	if (match)
		dasm_restore(dasm, match->save);
	return match;
}

static bool _dasm_print_macro(struct dasm_state *dasm)
{
	if (!macros)
		create_macro_tree();

	int argptr = 0;
	int32_t args[MACRO_INSTRUCTIONS_MAX];

	// match as many instructions as possible
	struct macro_node *next, *node = macros;
	while ((next = find_node(node, dasm->instr->opcode))) {
		if (node != macros && !can_elide(dasm))
			return false;

		for (int i = 0; i < dasm->instr->nr_args; i++) {
			args[argptr++] = dasm_arg(dasm, i);
		}

		node = next;
		node->save = dasm_save(dasm);
		dasm_next(dasm);
	}

	// rewind to the last terminal node
	if (!node->check)
		node = match_rewind(dasm, node);
	else
		dasm_restore(dasm, node->save);

	// find the longest match that passes the argument check
	while (node && !node->check(dasm, args)) {
		node = match_rewind(dasm, node);
	}

	// no match
	if (!node)
		return false;

	fprintf(dasm->out, "%s ", node->name);
	node->emit(dasm, args);
	fputc('\n', dasm->out);
	return true;
}

bool dasm_print_macro(struct dasm_state *dasm)
{
	// XXX: quick fail check
	switch (dasm->instr->opcode) {
	case PUSHLOCALPAGE:
	case PUSHGLOBALPAGE:
	case PUSHSTRUCTPAGE:
		break;
	default:
		return false;
	}

	dasm_save_t save = dasm_save(dasm);
	bool success = _dasm_print_macro(dasm);
	if (!success)
		dasm_restore(dasm, save);
	return success;
}
