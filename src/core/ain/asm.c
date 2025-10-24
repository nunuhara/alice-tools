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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/file.h"
#include "system4/hashtable.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ain.h"
#include "asm.h"

KHASH_MAP_INIT_STR(string_ht, size_t);

// TODO: better error messages
#define ASM_ERROR(state, ...) ERROR(__VA_ARGS__)

#define MACRO(code, _name, nargs, _ip_inc, ...)	\
	[code - PSEUDO_OP_OFFSET] = {			\
		.opcode = (enum opcode)code,		\
		.name = _name,				\
		.nr_args = nargs,			\
		.ip_inc = _ip_inc,			\
		.implemented = false,			\
		.args = { __VA_ARGS__ }			\
	}

#define PSEUDO_OP(code, _name, nargs, ...)	\
	[code - PSEUDO_OP_OFFSET] = {		\
		.opcode = (enum opcode)code,	\
		.name = _name,			\
		.nr_args = nargs,		\
		.ip_inc = 0,			\
		.implemented = false,		\
		.args = { __VA_ARGS__ }		\
	}

struct instruction asm_pseudo_ops[NR_PSEUDO_OPS - PSEUDO_OP_OFFSET] = {
	PSEUDO_OP(PO_CASE,          ".CASE",              2),
	PSEUDO_OP(PO_STRCASE,       ".STRCASE",           2),
	PSEUDO_OP(PO_DEFAULT,       ".DEFAULT",           1),
	PSEUDO_OP(PO_SETSTR,        ".SETSTR",            2),
	PSEUDO_OP(PO_SETMSG,        ".SETMSG",            2),
	MACRO(PO_MSG,               ".MSG",               1, 6),
	MACRO(PO_LOCALREF,          ".LOCALREF",          1, 10),
	MACRO(PO_LOCALREFREF,       ".LOCALREFREF",       1, 10),
	MACRO(PO_LOCALINC,          ".LOCALINC",          1, 10),
	MACRO(PO_LOCALINC2,         ".LOCALINC2",         1, 20),
	MACRO(PO_LOCALINC3,         ".LOCALINC3",         1, 20),
	MACRO(PO_LOCALDEC,          ".LOCALDEC",          1, 10),
	MACRO(PO_LOCALDEC2,         ".LOCALDEC2",         1, 20),
	MACRO(PO_LOCALDEC3,         ".LOCALDEC3",         1, 20),
	MACRO(PO_LOCALPLUSA,        ".LOCALPLUSA",        2, 18),
	MACRO(PO_LOCALMINUSA,       ".LOCALMINUSA",       2, 18),
	MACRO(PO_LOCALASSIGN,       ".LOCALASSIGN",       2, 18),
	MACRO(PO_LOCALASSIGN2,      ".LOCALASSIGN2",      1, 14),
	MACRO(PO_F_LOCALASSIGN,     ".F_LOCALASSIGN",     2, 18),
	MACRO(PO_STACK_LOCALASSIGN, ".STACK_LOCALASSIGN", 1, 26),
	MACRO(PO_S_LOCALASSIGN,     ".S_LOCALASSIGN",     2, 20),
	MACRO(PO_LOCALDELETE,       ".LOCALDELETE",       1, 24),
	MACRO(PO_LOCALCREATE,       ".LOCALCREATE",       2, 34),
	MACRO(PO_GLOBALREF,         ".GLOBALREF",         1, 10),
	MACRO(PO_GLOBALREFREF,      ".GLOBALREFREF",      1, 10),
	MACRO(PO_GLOBALINC,         ".GLOBALINC",         1, 10),
	MACRO(PO_GLOBALDEC,         ".GLOBALDEC",         1, 10),
	MACRO(PO_GLOBALASSIGN,      ".GLOBALASSIGN",      2, 18),
	MACRO(PO_F_GLOBALASSIGN,    ".F_GLOBALASSIGN",    2, 18),
	MACRO(PO_STRUCTREF,         ".STRUCTREF",         2, 10),
	MACRO(PO_STRUCTREFREF,      ".STRUCTREFREF",      2, 10),
	MACRO(PO_STRUCTINC,         ".STRUCTINC",         2, 10),
	MACRO(PO_STRUCTDEC,         ".STRUCTDEC",         2, 10),
	MACRO(PO_STRUCTASSIGN,      ".STRUCTASSIGN",      3, 18),
	MACRO(PO_F_STRUCTASSIGN,    ".F_STRUCTASSIGN",    3, 18),
	MACRO(PO_PUSHVMETHOD,       ".PUSHVMETHOD",       2, 30),
};

struct string_table {
	struct string **strings;
	size_t size;
	size_t allocated;
};

#define ASM_FUNC_STACK_SIZE 16

struct asm_state {
	struct ain *ain;
	uint32_t flags;
	uint8_t *buf;
	size_t buf_ptr;
	size_t buf_len;
	int32_t func;
	int32_t func_stack[ASM_FUNC_STACK_SIZE];
	int32_t lib;
};

const_pure int32_t asm_instruction_width(int opcode)
{
	if (opcode >= PSEUDO_OP_OFFSET)
		return asm_pseudo_ops[opcode - PSEUDO_OP_OFFSET].ip_inc;
	return instruction_width(opcode);
}

static void init_asm_state(struct asm_state *state, struct ain *ain, uint32_t flags)
{
	// update offsets for version-dependent macro expansion
	if (AIN_VERSION_GTE(ain, 14, 0)) {
		asm_pseudo_ops[PO_LOCALREF      - PSEUDO_OP_OFFSET].ip_inc = 14;
		asm_pseudo_ops[PO_GLOBALREF     - PSEUDO_OP_OFFSET].ip_inc = 14;
		asm_pseudo_ops[PO_STRUCTREF     - PSEUDO_OP_OFFSET].ip_inc = 14;
		asm_pseudo_ops[PO_LOCALASSIGN   - PSEUDO_OP_OFFSET].ip_inc = 22;
		asm_pseudo_ops[PO_S_LOCALASSIGN - PSEUDO_OP_OFFSET].ip_inc = 36;
		asm_pseudo_ops[PO_GLOBALASSIGN  - PSEUDO_OP_OFFSET].ip_inc = 22;
		asm_pseudo_ops[PO_STRUCTASSIGN  - PSEUDO_OP_OFFSET].ip_inc = 22;
		asm_pseudo_ops[PO_LOCALCREATE   - PSEUDO_OP_OFFSET].ip_inc = 40;
		asm_pseudo_ops[PO_LOCALDELETE   - PSEUDO_OP_OFFSET].ip_inc = 36;
		asm_pseudo_ops[PO_LOCALINC2     - PSEUDO_OP_OFFSET].ip_inc = 34;
		asm_pseudo_ops[PO_LOCALDEC2     - PSEUDO_OP_OFFSET].ip_inc = 34;
	}

	memset(state, 0, sizeof(*state));
	state->ain = ain;
	state->flags = flags;
	state->func = -1;
	state->lib = -1;
}

static void asm_write_opcode(struct asm_state *state, uint16_t opcode)
{
//	if (state->buf_len - state->buf_ptr <= (size_t)instruction_width(opcode)) {
	if (state->buf_len - state->buf_ptr <= 18) {
		if (!state->buf) {
			state->buf_len = 4096;
			state->buf = xmalloc(state->buf_len);
		} else {
			state->buf_len = state->buf_len * 2;
			state->buf = xrealloc(state->buf, state->buf_len);
		}
	}

	state->buf[state->buf_ptr++] = opcode & 0xFF;
	state->buf[state->buf_ptr++] = (opcode & 0xFF00) >> 8;
}

static void asm_write_argument(struct asm_state *state, uint32_t arg)
{
	state->buf[state->buf_ptr++] = (arg & 0x000000FF);
	state->buf[state->buf_ptr++] = (arg & 0x0000FF00) >> 8;
	state->buf[state->buf_ptr++] = (arg & 0x00FF0000) >> 16;
	state->buf[state->buf_ptr++] = (arg & 0xFF000000) >> 24;
}

static void asm_write_instruction0(struct asm_state *state, uint16_t opcode)
{
	asm_write_opcode(state, opcode);
}

static void asm_write_instruction1(struct asm_state *state, uint16_t opcode, uint32_t arg0)
{
	asm_write_opcode(state, opcode);
	asm_write_argument(state, arg0);
}

static void asm_write_instruction2(struct asm_state *state, uint16_t opcode, uint32_t arg0, uint32_t arg1)
{
	asm_write_opcode(state, opcode);
	asm_write_argument(state, arg0);
	asm_write_argument(state, arg1);
}

const struct instruction *asm_get_instruction(const char *name)
{
	if (name[0] == '.') {
		for (int i = 0; i < NR_PSEUDO_OPS - PSEUDO_OP_OFFSET; i++) {
			if (!strcmp(name, asm_pseudo_ops[i].name))
				return &asm_pseudo_ops[i];
		}
	}
	for (int i = 0; i < NR_OPCODES; i++) {
		if (!strcmp(name, instructions[i].name))
			return &instructions[i];
	}
	return NULL;
}

static char *parse_identifier(possibly_unused struct asm_state *state, char *s, int *n)
{
	char *delim = strchr(s, '#');
	if (!delim) {
		*n = 0;
		return s;
	}

	*delim = '\0';
	delim++;

	char *endptr;
	long nn = strtol(delim, &endptr, 10);
	if (*delim && !*endptr) {
		*n = nn;
		return s;
	}

	*delim = '#';
	ASM_ERROR(state, "Invalid identifier: '%s' (bad suffix)", s);
}

static bool _parse_integer_constant(const char *arg, int32_t *out)
{
	char *endptr;
	errno = 0;
	long i = strtol(arg, &endptr, 0);
	if (errno || *endptr != '\0')
		return false;
	*out = i;
	return true;
}

static int32_t parse_integer_constant(struct asm_state *state, const char *arg)
{
	int i = 0;
	if (!_parse_integer_constant(arg, &i))
		ASM_ERROR(state, "Invalid integer constant: '%s'", arg);
	return i;
}

static void realloc_switch_table(struct ain *ain, int i)
{
	if (i < ain->nr_switches)
		return;
	ain->switches = xrealloc_array(ain->switches, ain->nr_switches, i+1, sizeof(struct ain_switch));
	ain->nr_switches = i+1;
}

static void realloc_switch_cases(struct ain_switch *swi, int i)
{
	if (i < swi->nr_cases)
		return;
	swi->cases = xrealloc_array(swi->cases, swi->nr_cases, i+1, sizeof(struct ain_switch_case));
	swi->nr_cases = i+1;
}

static void realloc_string_table(struct ain *ain, int i)
{
	if (i < ain->nr_strings)
		return;
	ain->strings = xrealloc_array(ain->strings, ain->nr_strings, i+1, sizeof(struct string*));
	ain->nr_strings = i+1;
}

static void realloc_message_table(struct ain *ain, int i)
{
	if (i < ain->nr_messages)
		return;
	ain->messages = xrealloc_array(ain->messages, ain->nr_messages, i+1, sizeof(struct string*));
	ain->nr_messages = i+1;
}

static int asm_add_string(struct asm_state *state, const char *str)
{
	char *sjis = conv_output(str);
	int no = ain_add_string(state->ain, sjis);
	free(sjis);
	return no;
}

static uint32_t asm_resolve_arg(struct asm_state *state, enum opcode opcode, enum instruction_argtype type, const char *arg)
{
	if (state->flags & ASM_RAW)
		type = T_INT;

	struct ain *ain = state->ain;
	switch (type) {
	case T_INT:
		return parse_integer_constant(state, arg);
	case T_FLOAT: {
		char *endptr;
		union { int32_t i; float f; } v;
		errno = 0;
		v.f = strtof(arg, &endptr);
		if (errno || *endptr != '\0')
			ASM_ERROR(state, "Invalid float: %s", arg);
		return v.i;
	}
	case T_SWITCH: {
		int i = parse_integer_constant(state, arg);
		if (i < 0)
			ASM_ERROR(state, "Invalid switch number: %d", i);
		realloc_switch_table(ain, i);
		if (opcode == SWITCH)
			ain->switches[i].case_type = AIN_SWITCH_INT;
		else if (opcode == STRSWITCH)
			ain->switches[i].case_type = AIN_SWITCH_STRING;
		return i;
	}
	case T_ADDR: {
		khiter_t k;
		k = kh_get(label_table, label_table, arg);
		if (k == kh_end(label_table))
			ASM_ERROR(state, "Unable to resolve label: '%s'", arg);
		return kh_value(label_table, k);
	}
	case T_FUNC: {
		char *u = conv_output(arg);
		int fno = ain_get_function(ain, u);
		free(u);
		if (fno < 0)
			ASM_ERROR(state, "Unable to resolve function: '%s'", arg);
		return fno;
	}
	case T_STRING: {
		return asm_add_string(state, arg);
	}
	case T_MSG: {
		int32_t i = parse_integer_constant(state, arg);
		if (i < 0)
			ASM_ERROR(state, "Message index out of bounds: '%s'", arg);
		realloc_message_table(ain, i);
		return i;
	}
	case T_LOCAL: {
		int n, count = 0;
		char *u = conv_output(arg);
		u = parse_identifier(state, u, &n);
		struct ain_function *f = &state->ain->functions[state->func];
		for (int i = 0; i < f->nr_vars; i++) {
			if (!strcmp(u, f->vars[i].name)) {
				if (count < n) {
					count++;
					continue;
				}
				free(u);
				return i;
			}
		}
		ASM_ERROR(state, "Unable to resolve local variable: '%s'", arg);
	}
	case T_GLOBAL: {
		char *u = conv_output(arg);
		for (int i = 0; i < state->ain->nr_globals; i++) {
			if (!strcmp(u, state->ain->globals[i].name)) {
				free(u);
				return i;
			}
		}
		ASM_ERROR(state, "Unable to resolve global variable: '%s'", arg);
	}
	case T_STRUCT: {
		char *u = conv_output(arg);
		int sno = ain_get_struct(state->ain, u);
		free(u);
		if (sno < 0)
			ASM_ERROR(state, "Unable to resolve struct: '%s'", arg);
		return sno;
	}
	case T_SYSCALL: {
		for (int i = 0; i < NR_SYSCALLS; i++) {
			if (!strcmp(arg, syscalls[i].name))
				return i;
		}
		ASM_ERROR(state, "Unable to resolve system call: '%s'", arg);
	}
	case T_HLL: {
		for (int i = 0; i < state->ain->nr_libraries; i++) {
			if (!strcmp(arg, state->ain->libraries[i].name)) {
				state->lib = i;
				return i;
			}
		}
		ASM_ERROR(state, "Unable to resolve library: '%s'", arg);
	}
	case T_HLLFUNC: {
		if (state->lib < 0)
			ERROR("Tried to resolve library function without active library?");
		int n, count = 0;
		char *u = conv_output(arg);
		u = parse_identifier(state, u, &n);
		for (int i = 0; i < state->ain->libraries[state->lib].nr_functions; i++) {
			if (strcmp(u, state->ain->libraries[state->lib].functions[i].name))
				continue;
			if (count < n) {
				count++;
				continue;
			}
			state->lib = -1;
			free(u);
			return i;
		}
		ASM_ERROR(state, "Unable to resolve library function: '%s.%s'",
			  state->ain->libraries[state->lib].name, arg);
	}
	case T_FILE: {
		if (!state->ain->nr_filenames)
			return atoi(arg);
		char *u = conv_output(arg);
		for (int i = 0; i < state->ain->nr_filenames; i++) {
			if (!strcmp(u, state->ain->filenames[i])) {
				free(u);
				return i;
			}
		}
		ASM_ERROR(state, "Unable to resolve filename: '%s'", arg);
	}
	case T_DLG: {
		char *u = conv_output(arg);
		for (int i = 0; i < state->ain->nr_delegates; i++) {
			if (!strcmp(u, state->ain->delegates[i].name)) {
				free(u);
				return i;
			}
		}
		ASM_ERROR(state, "Unable to resolve delegate: '%s'", arg);
	}
	default:
		ASM_ERROR(state, "Unhandled argument type: %d", type);
	}
}

static void decompose_switch_index(struct asm_state *state, char *in, int *switch_out, int *case_out)
{
	char *tmp = strchr(in, ':');
	if (!tmp)
		ASM_ERROR(state, "Invalid switch:case index: '%s'", in);

	*tmp++ = '\0';

	int n_switch = parse_integer_constant(state, in);
	int n_case = parse_integer_constant(state, tmp);
	if (n_switch < 0 || n_switch >= state->ain->nr_switches)
		ASM_ERROR(state, "Invalid switch index: %d", n_switch);
	if (n_case < 0 || n_case >= state->ain->switches[n_switch].nr_cases)
		ASM_ERROR(state, "Invalid case index: %d", n_case);

	*switch_out = n_switch;
	*case_out = n_case;
}

static int find_member(struct ain_struct *s, char *_member_name)
{
	int member_no = -1;
	char *member_name = conv_output(_member_name);

	for (int i = 0; i < s->nr_members; i++) {
		if (!strcmp(member_name, s->members[i].name)) {
			member_no = i;
			break;
		}
	}
	free(member_name);
	return member_no;
}

static int get_member_no(struct asm_state *state, char *struct_name, char *_member_name)
{
	int struct_no = asm_resolve_arg(state, PUSH, T_STRUCT, struct_name);
	int member_no = find_member(&state->ain->structures[struct_no], _member_name);

	if (member_no < 0) {
		char *sname = conv_utf8(struct_name);
		char *mname = conv_utf8(_member_name);
		ASM_ERROR(state, "Unable to resolve struct member: %s.%s", sname, mname);
	}

	return member_no;
}

static int get_vtable_no(struct asm_state *state)
{
	int struct_type = state->ain->functions[state->func].struct_type;
	if (struct_type < 0)
		ASM_ERROR(state, ".PUSHVMETHOD macro in non-member function");

	int member_no = find_member(&state->ain->structures[struct_type], "<vtable>");

	if (member_no < 0) {
		ASM_ERROR(state, "Unable to resolve vtable");
	}

	return member_no;
}

void handle_pseudo_op(struct asm_state *state, struct parse_instruction *instr)
{
	switch (instr->opcode) {
	case PO_CASE: {
		int n_switch, n_case, c;
		decompose_switch_index(state, kv_A(*instr->args, 0)->text, &n_switch, &n_case);

		struct ain_switch *swi = &state->ain->switches[n_switch];
		c = parse_integer_constant(state, kv_A(*instr->args, 1)->text);
		realloc_switch_cases(swi, n_case);
		swi->cases[n_case].address = state->buf_ptr;
		swi->cases[n_case].value = c;
		break;
	}
	case PO_STRCASE: {
		int n_switch, n_case, c;
		decompose_switch_index(state, kv_A(*instr->args, 0)->text, &n_switch, &n_case);

		struct ain_switch *swi = &state->ain->switches[n_switch];
		c = asm_add_string(state, kv_A(*instr->args, 1)->text);
		realloc_switch_cases(swi, n_case);
		swi->cases[n_case].address = state->buf_ptr;
		swi->cases[n_case].value = c;
		break;
	}
	case PO_DEFAULT: {
		int n_switch = parse_integer_constant(state, kv_A(*instr->args, 0)->text);
		if (n_switch < 0 || n_switch >= state->ain->nr_switches)
			ASM_ERROR(state, "Invalid switch index: %d", n_switch);
		state->ain->switches[n_switch].default_address = state->buf_ptr;
		break;
	}
	case PO_SETSTR: {
		int n_str = parse_integer_constant(state, kv_A(*instr->args, 0)->text);
		if (n_str < 0)
			ASM_ERROR(state, "Invalid string index: %d", n_str);
		realloc_string_table(state->ain, n_str);
		if (state->ain->strings[n_str])
			free_string(state->ain->strings[n_str]);
		char *sjis = conv_output(kv_A(*instr->args, 1)->text);
		state->ain->strings[n_str] = make_string(sjis, strlen(sjis));
		free(sjis);
		break;
	}
	case PO_SETMSG: {
		int n_msg = parse_integer_constant(state, kv_A(*instr->args, 0)->text);
		if (n_msg < 0)
			ASM_ERROR(state, "Invalid message index: %d", n_msg);
		realloc_message_table(state->ain, n_msg);
		if (state->ain->messages[n_msg])
			free_string(state->ain->messages[n_msg]);
		char *sjis = conv_output(kv_A(*instr->args, 1)->text);
		state->ain->messages[n_msg] = make_string(sjis, strlen(sjis));
		free(sjis);
		break;
	}
	case PO_MSG: {
		int n_msg = state->ain->nr_messages;
		realloc_message_table(state->ain, n_msg);
		char *sjis = conv_output(kv_A(*instr->args, 0)->text);
		state->ain->messages[n_msg] = make_string(sjis, strlen(sjis));
		free(sjis);

		asm_write_instruction1(state, _MSG, n_msg);
		break;
	}
	case PO_LOCALREF: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_REF, 1);
		} else {
			asm_write_instruction0(state, REF);
		}
		break;
	}
	case PO_GLOBALREF: {
		int32_t var = asm_resolve_arg(state, PUSH, T_GLOBAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHGLOBALPAGE);
		asm_write_instruction1(state, PUSH, var);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_REF, 1);
		} else {
			asm_write_instruction0(state, REF);
		}
		break;
	}
	case PO_LOCALREFREF: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, REFREF);
		break;
	}
	case PO_GLOBALREFREF: {
		int32_t var = asm_resolve_arg(state, PUSH, T_GLOBAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHGLOBALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, REFREF);
		break;
	}
	case PO_LOCALINC: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, INC);
		break;
	}
	case PO_LOCALINC2: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_DUP, 2);
			asm_write_instruction1(state, X_REF, 1);
			asm_write_instruction2(state, X_MOV, 3, 1);
			asm_write_instruction0(state, INC);
			asm_write_instruction0(state, POP);
		} else {
			asm_write_instruction0(state, DUP2);
			asm_write_instruction0(state, REF);
			asm_write_instruction0(state, DUP_X2);
			asm_write_instruction0(state, POP);
			asm_write_instruction0(state, INC);
			asm_write_instruction0(state, POP);
		}
		break;
	}
	case PO_LOCALINC3: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction1(state, X_DUP, 2);
		asm_write_instruction0(state, INC);
		asm_write_instruction0(state, POP);
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_LOCALDEC: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, DEC);
		break;
	}
	case PO_LOCALDEC2: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_DUP, 2);
			asm_write_instruction1(state, X_REF, 1);
			asm_write_instruction2(state, X_MOV, 3, 1);
			asm_write_instruction0(state, DEC);
			asm_write_instruction0(state, POP);
		} else {
			asm_write_instruction0(state, DUP2);
			asm_write_instruction0(state, REF);
			asm_write_instruction0(state, DUP_X2);
			asm_write_instruction0(state, POP);
			asm_write_instruction0(state, DEC);
			asm_write_instruction0(state, POP);
		}
		break;
	}
	case PO_LOCALDEC3: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction1(state, X_DUP, 2);
		asm_write_instruction0(state, DEC);
		asm_write_instruction0(state, POP);
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_LOCALPLUSA: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		int32_t val = asm_resolve_arg(state, PUSH, T_INT, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction1(state, PUSH, val);
		asm_write_instruction0(state, PLUSA);
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_LOCALMINUSA: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		int32_t val = asm_resolve_arg(state, PUSH, T_INT, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction1(state, PUSH, val);
		asm_write_instruction0(state, MINUSA);
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_LOCALASSIGN: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		int32_t val = asm_resolve_arg(state, PUSH, T_INT, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction1(state, PUSH, val);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_ASSIGN, 1);
		} else {
			asm_write_instruction0(state, ASSIGN);
		}
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_LOCALASSIGN2: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction0(state, SWAP);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, SWAP);
		asm_write_instruction0(state, ASSIGN);
		break;
	}
	case PO_F_LOCALASSIGN: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		int32_t val = asm_resolve_arg(state, F_PUSH, T_FLOAT, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction1(state, F_PUSH, val);
		asm_write_instruction0(state, F_ASSIGN);
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_STACK_LOCALASSIGN: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, REF);
		asm_write_instruction0(state, DELETE);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction0(state, SWAP);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, SWAP);
		asm_write_instruction0(state, ASSIGN);
		break;
	}
	case PO_S_LOCALASSIGN: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		int32_t str = asm_resolve_arg(state, S_PUSH, T_STRING, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_DUP, 2);
			asm_write_instruction1(state, X_REF, 1);
			asm_write_instruction0(state, DELETE);
			asm_write_instruction1(state, S_PUSH, str);
			asm_write_instruction1(state, X_ASSIGN, 1);
			asm_write_instruction0(state, POP);
		} else {
			asm_write_instruction0(state, REF);
			asm_write_instruction1(state, S_PUSH, str);
			asm_write_instruction0(state, S_ASSIGN);
			asm_write_instruction0(state, DELETE);
		}
		break;
	}
	case PO_LOCALDELETE: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_DUP, 2);
			asm_write_instruction1(state, X_REF, 1);
			asm_write_instruction0(state, DELETE);
			asm_write_instruction1(state, PUSH, -1);
			asm_write_instruction1(state, X_ASSIGN, 1);
			asm_write_instruction0(state, POP);
		} else {
			asm_write_instruction0(state, DUP2);
			asm_write_instruction0(state, REF);
			asm_write_instruction0(state, DELETE);
			asm_write_instruction1(state, PUSH, -1);
			asm_write_instruction0(state, ASSIGN);
			asm_write_instruction0(state, POP);
		}
		break;
	}
	case PO_LOCALCREATE: {
		int32_t var = asm_resolve_arg(state, PUSH, T_LOCAL, kv_A(*instr->args, 0)->text);
		int32_t struc = asm_resolve_arg(state, NEW, T_STRUCT, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHLOCALPAGE);
		asm_write_instruction1(state, PUSH, var);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_DUP, 2);
			asm_write_instruction1(state, X_REF, 1);
			asm_write_instruction0(state, DELETE);
			asm_write_instruction2(state, NEW, struc, -1);
			asm_write_instruction1(state, X_ASSIGN, 1);
			asm_write_instruction0(state, POP);
		} else {
			asm_write_instruction0(state, DUP2);
			asm_write_instruction0(state, REF);
			asm_write_instruction0(state, DELETE);
			asm_write_instruction0(state, DUP2);
			asm_write_instruction2(state, NEW, struc, -1);
			asm_write_instruction0(state, ASSIGN);
			asm_write_instruction0(state, POP);
			asm_write_instruction0(state, POP);
			asm_write_instruction0(state, POP);
		}
		break;
	}
	case PO_GLOBALINC: {
		int32_t var = asm_resolve_arg(state, PUSH, T_GLOBAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHGLOBALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, INC);
		break;
	}
	case PO_GLOBALDEC: {
		int32_t var = asm_resolve_arg(state, PUSH, T_GLOBAL, kv_A(*instr->args, 0)->text);
		asm_write_instruction0(state, PUSHGLOBALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction0(state, DEC);
		break;
	}
	case PO_GLOBALASSIGN: {
		int32_t var = asm_resolve_arg(state, PUSH, T_GLOBAL, kv_A(*instr->args, 0)->text);
		int32_t val = asm_resolve_arg(state, PUSH, T_INT, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHGLOBALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction1(state, PUSH, val);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_ASSIGN, 1);
		} else {
			asm_write_instruction0(state, ASSIGN);
		}
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_F_GLOBALASSIGN: {
		int32_t var = asm_resolve_arg(state, PUSH, T_GLOBAL, kv_A(*instr->args, 0)->text);
		int32_t val = asm_resolve_arg(state, F_PUSH, T_FLOAT, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHGLOBALPAGE);
		asm_write_instruction1(state, PUSH, var);
		asm_write_instruction1(state, F_PUSH, val);
		asm_write_instruction0(state, F_ASSIGN);
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_STRUCTREF: {
		int32_t memb = get_member_no(state, kv_A(*instr->args, 0)->text, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHSTRUCTPAGE);
		asm_write_instruction1(state, PUSH, memb);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_REF, 1);
		} else {
			asm_write_instruction0(state, REF);
		}
		break;
	}
	case PO_STRUCTREFREF: {
		int32_t memb = get_member_no(state, kv_A(*instr->args, 0)->text, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHSTRUCTPAGE);
		asm_write_instruction1(state, PUSH, memb);
		asm_write_instruction0(state, REFREF);
		break;
	}
	case PO_STRUCTINC: {
		int32_t memb = get_member_no(state, kv_A(*instr->args, 0)->text, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHSTRUCTPAGE);
		asm_write_instruction1(state, PUSH, memb);
		asm_write_instruction0(state, INC);
		break;
	}
	case PO_STRUCTDEC: {
		int32_t memb = get_member_no(state, kv_A(*instr->args, 0)->text, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHSTRUCTPAGE);
		asm_write_instruction1(state, PUSH, memb);
		asm_write_instruction0(state, DEC);
		break;
	}
	case PO_STRUCTASSIGN: {
		int32_t memb = get_member_no(state, kv_A(*instr->args, 0)->text, kv_A(*instr->args, 1)->text);
		int32_t val = asm_resolve_arg(state, PUSH, T_INT, kv_A(*instr->args, 2)->text);
		asm_write_instruction0(state, PUSHSTRUCTPAGE);
		asm_write_instruction1(state, PUSH, memb);
		asm_write_instruction1(state, PUSH, val);
		if (AIN_VERSION_GTE(state->ain, 14, 0)) {
			asm_write_instruction1(state, X_ASSIGN, 1);
		} else {
			asm_write_instruction0(state, ASSIGN);
		}
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_F_STRUCTASSIGN: {
		int32_t memb = get_member_no(state, kv_A(*instr->args, 0)->text, kv_A(*instr->args, 1)->text);
		int32_t val = asm_resolve_arg(state, F_PUSH, T_FLOAT, kv_A(*instr->args, 2)->text);
		asm_write_instruction0(state, PUSHSTRUCTPAGE);
		asm_write_instruction1(state, PUSH, memb);
		asm_write_instruction1(state, F_PUSH, val);
		asm_write_instruction0(state, F_ASSIGN);
		asm_write_instruction0(state, POP);
		break;
	}
	case PO_PUSHVMETHOD: {
		int32_t memb = asm_resolve_arg(state, PUSH, T_INT, kv_A(*instr->args, 0)->text);
		int32_t val = asm_resolve_arg(state, PUSH, T_INT, kv_A(*instr->args, 1)->text);
		asm_write_instruction0(state, PUSHSTRUCTPAGE);
		asm_write_instruction1(state, PUSH, memb);
		asm_write_instruction0(state, DUP_U2);
		asm_write_instruction1(state, PUSH, get_vtable_no(state));
		asm_write_instruction0(state, REF);
		asm_write_instruction0(state, SWAP);
		asm_write_instruction1(state, PUSH, val);
		asm_write_instruction0(state, ADD);
		asm_write_instruction0(state, REF);
		break;
	}
	}
}

static void validate_ain(struct ain *ain)
{
	for (int i = 0; i < ain->nr_strings; i++) {
		if (ain->strings[i])
			continue;
		WARNING("String 0x%x unallocated", i);
		ain->strings[i] = make_string("", 0);
	}

	for (int i = 0; i < ain->nr_messages; i++) {
		if (ain->messages[i])
			continue;
		WARNING("Message 0x%x unallocated", i);
		ain->messages[i] = make_string("", 0);
	}

	for (int i = 0; i < ain->nr_switches; i++) {
		if (!ain->switches[i].nr_cases)
			WARNING("Switch 0x%x unallocated", i);
	}

	if (ain->main < 0 || ain->main >= ain->nr_functions)
		ERROR("Invalid main function: %d", ain->main);
	if (strcmp(ain->functions[ain->main].name, "main"))
		WARNING("Main function is not named 'main': '%s'", ain->functions[ain->main].name);

	if (ain->MSGF.present) {
		if (ain->msgf < 0 || ain->msgf >= ain->nr_functions)
			ERROR("Invalid message function: %d", ain->msgf);
		if (strcmp(ain->functions[ain->msgf].name, "message"))
			WARNING("Message function is not named 'message': '%s' (%d)", ain->functions[ain->msgf].name, ain->msgf);
	}
}

static void _asm_enter_function(struct asm_state *state, int32_t fno)
{
	if (fno < 0 || fno >= state->ain->nr_functions)
		ASM_ERROR(state, "Invalid function number: %d", fno);

	for (int i = 1; i < ASM_FUNC_STACK_SIZE; i++) {
		state->func_stack[i] = state->func_stack[i-1];
	}
	state->func_stack[0] = state->func;
	state->func = fno;
}

static void asm_enter_function(struct asm_state *state, int32_t fno)
{
	_asm_enter_function(state, fno);
	state->ain->functions[fno].address = state->buf_ptr + 6;
	asm_write_opcode(state, FUNC);
	asm_write_argument(state, fno);
}

static void asm_leave_function(struct asm_state *state)
{
	state->func = state->func_stack[0];
	for (int i = 1; i < ASM_FUNC_STACK_SIZE; i++) {
		state->func_stack[i-1] = state->func_stack[i];
	}
}

static parse_instruction_list *jam_parse(const char *filename, uint32_t instr_ptr)
{
	current_line_nr = &asm_line;
	current_file_name = &filename;

	if (!strcmp(filename, "-"))
		asm_in = stdin;
	else
		asm_in = file_open_utf8(filename, "r");
	if (!asm_in)
		ERROR("Opening input file '%s': %s", filename, strerror(errno));

	label_table = kh_init(label_table);
	asm_instr_ptr = instr_ptr;
	asm_parse();
	return parsed_code;
}

static void jam_assemble(struct asm_state *state, const char *filename);

static void jam_inject(struct asm_state *state, const char *filename, int fno, unsigned offset)
{
	assert(fno >= 0 && fno < state->ain->nr_functions);
	struct ain_function *f = &state->ain->functions[fno];
	struct dasm_state dasm;
	dasm_init(&dasm, NULL, state->ain, 0);

	unsigned inject_addr = f->address + offset;

	// write function up to offset (fixing label offsets)
	asm_write_instruction1(state, FUNC, fno);
	unsigned new_func_addr = state->buf_ptr;
	unsigned label_offset = new_func_addr - f->address;
	for (dasm_jump(&dasm, f->address); dasm.addr < inject_addr && !dasm_eof(&dasm); dasm_next(&dasm)) {
		asm_write_opcode(state, dasm.instr->opcode);
		for (int i = 0; i < dasm.instr->nr_args; i++) {
			if (dasm.instr->args[i] == T_ADDR) {
				unsigned addr = dasm_arg(&dasm, i);
				asm_write_argument(state, addr + label_offset);
			} else {
				asm_write_argument(state, dasm_arg(&dasm, i));
			}
		}
	}
	if (dasm.addr != inject_addr)
		ALICE_ERROR("Invalid injection offset: %u", offset);

	// inject code
	inject_addr = state->buf_ptr;
	_asm_enter_function(state, fno);
	jam_assemble(state, filename);
	asm_leave_function(state);
	label_offset += state->buf_ptr - inject_addr;

	// write remainder of function (fixing label offsets)
	for (; !dasm_eof(&dasm); dasm_next(&dasm)) {
		asm_write_opcode(state, dasm.instr->opcode);
		for (int i = 0; i < dasm.instr->nr_args; i++) {
			if (dasm.instr->args[i] == T_ADDR) {
				unsigned addr = dasm_arg(&dasm, i);
				asm_write_argument(state, addr + label_offset);
			} else {
				asm_write_argument(state, dasm_arg(&dasm, i));
			}
		}
		if (dasm.instr->opcode == ENDFUNC)
			break;
	}

	f->address = new_func_addr;
}

static void _jam_assemble(struct asm_state *state, parse_instruction_list *code, size_t i)
{
	for (; i < kv_size(*code); i++) {
		struct parse_instruction *instr = kv_A(*code, i);
		if (instr->opcode >= PSEUDO_OP_OFFSET) {
			handle_pseudo_op(state, instr);
			continue;
		}

		struct instruction *idef = &instructions[instr->opcode];

		// NOTE: special case: we need to record the new function address in the ain structure
		if (idef->opcode == FUNC) {
			int32_t fno = 0;
			const char *arg = kv_A(*instr->args, 0)->text;
			if (!_parse_integer_constant(arg, &fno)) {
				fno = asm_resolve_arg(state, FUNC, T_FUNC, arg);
			}
			asm_enter_function(state, fno);
			continue;
		} else if (idef->opcode == ENDFUNC) {
			asm_leave_function(state);
		}

		asm_write_opcode(state, instr->opcode);
		for (int a = 0; a < idef->nr_args; a++) {
			asm_write_argument(state, asm_resolve_arg(state, idef->opcode, idef->args[a], kv_A(*instr->args, a)->text));
		}
	}
}

static void jam_assemble(struct asm_state *state, const char *filename)
{
	parse_instruction_list *code = jam_parse(filename, state->buf_ptr);
	_jam_assemble(state, code, 0);

	for (size_t i = 0; i < kv_size(*code); i++) {
		struct parse_instruction *instr = kv_A(*code, i);
		if (!instr->args) {
			free(instr);
			continue;
		}
		for (size_t a = 0; a < kv_size(*instr->args); a++) {
			free_string(kv_A(*instr->args, a));
		}
		kv_destroy(*instr->args);
		free(instr->args);
		free(instr);
	}
	kv_destroy(*code);
	free(code);

	const char *key;
	possibly_unused uint32_t val;
	kh_foreach(label_table, key, val, { free((char*)key); });
	kh_destroy(label_table, label_table);
}

/*
 * Assemble .jam file to replace entire code section.
 */
void ain_assemble_jam(const char *filename, struct ain *ain, uint32_t flags)
{
	// assemble .jam
	struct asm_state state;
	init_asm_state(&state, ain, flags);
	jam_assemble(&state, filename);

	// replace code section
	free(ain->code);
	ain->code = state.buf;
	ain->code_size = state.buf_ptr;

	if (!(flags & ASM_NO_VALIDATE))
		validate_ain(ain);
}

/*
 * Assemble .jam file as a patch (can be used to modify specific functions, etc).
 */
void ain_append_jam(const char *filename, struct ain *ain, int32_t flags)
{
	// assemble .jam
	struct asm_state state;
	init_asm_state(&state, ain, flags);
	state.buf = ain->code;
	state.buf_len = ain->code_size;
	state.buf_ptr = ain->code_size;
	jam_assemble(&state, filename);

	ain->code = state.buf;
	ain->code_size = state.buf_ptr;

	if (!(flags & ASM_NO_VALIDATE))
		validate_ain(ain);
}

/*
 * Assemble a .jam file and inject the code into an existing function.
 */
void ain_inject_jam(const char *filename, struct ain *ain, char *function, unsigned offset, int32_t flags)
{
	struct asm_state state;
	init_asm_state(&state, ain, flags);
	state.buf = xmalloc(ain->code_size);
	memcpy(state.buf, ain->code, ain->code_size);
	state.buf_len = ain->code_size;
	state.buf_ptr = ain->code_size;

	int fno = ain_get_function(ain, function);
	if (fno < 0) {
		ERROR("Unable to resolve function: %s", function);
	}
	jam_inject(&state, filename, fno, offset);

	free(ain->code);
	ain->code = state.buf;
	ain->code_size = state.buf_ptr;

	validate_ain(ain);
}
