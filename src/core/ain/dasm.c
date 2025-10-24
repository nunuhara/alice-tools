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

#include <stdio.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/port.h"
#include "khash.h"
#include "kvec.h"
#include "little_endian.h"

enum jump_target_type {
	JMP_LABEL,
	JMP_CASE,
	JMP_DEFAULT
};

struct jump_target {
	enum jump_target_type type;
	union {
		char *label;
		struct ain_switch_case *switch_case;
		struct ain_switch *switch_default;
	};
};

kv_decl(jump_list, struct jump_target*);

KHASH_MAP_INIT_INT(jump_table, jump_list*);
khash_t(jump_table) *jump_table;

static void jump_table_init(void)
{
	jump_table = kh_init(jump_table);
}

static void free_jump_targets(jump_list *list)
{
	if (!list)
		return;
	for (size_t i = 0; i < kv_size(*list); i++) {
		struct jump_target *t = kv_A(*list, i);
		if (t->type == JMP_LABEL)
			free(t->label);
		free(t);
	}
	kv_destroy(*list);
	free(list);
}

static void jump_table_fini(void)
{
	jump_list *list;
	kh_foreach_value(jump_table, list, free_jump_targets(list));
	kh_destroy(jump_table, jump_table);
}

static jump_list *get_jump_targets(ain_addr_t addr)
{
	khiter_t k = kh_get(jump_table, jump_table, addr);
	if (k == kh_end(jump_table))
		return NULL;
	return kh_value(jump_table, k);
}

bool dasm_is_jump_target(struct dasm_state *dasm)
{
	khiter_t k = kh_get(jump_table, jump_table, dasm->addr);
	return k != kh_end(jump_table);
}

static char *get_label(ain_addr_t addr)
{
	jump_list *list = get_jump_targets(addr);
	for (size_t i = 0; i < kv_size(*list); i++) {
		struct jump_target *t = kv_A(*list, i);
		if (t->type == JMP_LABEL)
			return t->label;
	}
	return NULL;
}

static void add_jump_target(struct jump_target *target, ain_addr_t addr)
{
	int ret;
	khiter_t k = kh_put(jump_table, jump_table, addr, &ret);
	if (!ret) {
		// add to list
		jump_list *list = kh_value(jump_table, k);
		kv_push(struct jump_target*, *list, target);
	} else if (ret == 1) {
		// create list
		jump_list *list = xmalloc(sizeof(jump_list));
		kv_init(*list);
		kv_push(struct jump_target*, *list, target);
		kh_value(jump_table, k) = list;
	} else {
		WARNING("Failed to insert target into jump table (%d)", ret);
	}
}

static void add_label(char *name, ain_addr_t addr)
{
	// check for duplicate label
	jump_list *list = get_jump_targets(addr);
	if (list) {
		for (size_t i = 0; i < kv_size(*list); i++) {
			struct jump_target *t = kv_A(*list, i);
			if (t->type == JMP_LABEL) {
				free(name);
				return;
			}
		}
	}

	struct jump_target *t = xmalloc(sizeof(struct jump_target));
	t->type = JMP_LABEL;
	t->label = name;
	add_jump_target(t, addr);
}

static void add_switch_case(struct ain_switch_case *c)
{
	struct jump_target *t = xmalloc(sizeof(struct jump_target));
	t->type = JMP_CASE;
	t->switch_case = c;
	add_jump_target(t, c->address);
}

static void add_switch_default(struct ain_switch *s)
{
	if (s->default_address == -1)
		return;
	struct jump_target *t = xmalloc(sizeof(struct jump_target));
	t->type = JMP_DEFAULT;
	t->switch_default = s;
	add_jump_target(t, s->default_address);
}

union float_cast {
	int32_t i;
	float f;
};

static float arg_to_float(int32_t arg)
{
	union float_cast v;
	v.i = arg;
	return v.f;
}

static void print_sjis(struct dasm_state *dasm, const char *s)
{
	char *u = conv_output(s);
	port_printf(dasm->port, "%s", u);
	free(u);
}

void dasm_print_string(struct dasm_state *dasm, const char *str)
{
	char *u = escape_string(str);
	port_printf(dasm->port, "\"%s\"", u);
	free(u);
}

void dasm_print_identifier(struct dasm_state *dasm, const char *str)
{
	char *u = conv_utf8(str);
	if (strchr(u, ' '))
		dasm_print_string(dasm, str);
	else
		print_sjis(dasm, str);
	free(u);
}

void dasm_print_local_variable(struct dasm_state *dasm, struct ain_function *func, int varno)
{
	int dup_no = 0; // nr of duplicate-named variables preceding varno
	for (int i = 0; i < func->nr_vars; i++) {
		if (i == varno)
			break;
		if (!strcmp(func->vars[i].name, func->vars[varno].name))
			dup_no++;
	}

	// if variable name is ambiguous, add #n suffix
	char *name;
	char buf[512];
	if (dup_no) {
		snprintf(buf, 512, "%s#%d", func->vars[varno].name, dup_no);
		name = buf;
	} else {
		name = func->vars[varno].name;
	}

	dasm_print_identifier(dasm, name);
}

static void print_function_name(struct dasm_state *dasm, struct ain_function *func)
{
	int i = ain_get_function_index(dasm->ain, func);

	char *name = func->name;
	char buf[512];
	if (i > 0) {
		snprintf(buf, 512, "%s#%d", func->name, i);
		name = buf;
	}

	dasm_print_identifier(dasm, name);
}

static void print_hll_function_name(struct dasm_state *dasm, struct ain_library *lib, int fno)
{
	int dup_no = 0;
	for (int i = 0; i < lib->nr_functions; i++) {
		if (i == fno)
			break;
		if (!strcmp(lib->functions[i].name, lib->functions[fno].name))
			dup_no++;
	}

	char *name = lib->functions[fno].name;
	char buf[512];
	if (dup_no) {
		snprintf(buf, 512, "%s#%d", lib->functions[fno].name, dup_no);
		name = buf;
	}

	dasm_print_identifier(dasm, name);
}

#define DASM_PRINT_ERROR(dasm, fmt, ...) \
	do { \
		if (dasm->flags & DASM_WARN_ON_ERROR) { \
			DASM_WARNING(dasm, fmt, ##__VA_ARGS__); \
			port_printf(dasm->port, "<" fmt ">", ##__VA_ARGS__); \
		} else { \
			DASM_ERROR(dasm, fmt, ##__VA_ARGS__); \
		} \
	} while (0)

static void print_argument(struct dasm_state *dasm, int32_t arg, enum instruction_argtype type, possibly_unused const char **comment)
{
	if (dasm->flags & DASM_NO_IDENTIFIERS) {
		port_printf(dasm->port, "0x%x", arg);
		return;
	}
	char *label;
	struct ain *ain = dasm->ain;
	switch (type) {
	case T_INT:
	case T_SWITCH:
		port_printf(dasm->port, "%d", arg);
		break;
	case T_FLOAT:
		port_printf(dasm->port, "%f", arg_to_float(arg));
		break;
	case T_ADDR:
		label = get_label(arg);
		if (!label) {
			WARNING("No label generated for address: 0x%x", arg);
			port_printf(dasm->port, "0x%x", arg);
		} else {
			port_printf(dasm->port, "%s", label);
		}
		break;
	case T_FUNC:
		if (arg < 0 || arg >= ain->nr_functions)
			DASM_PRINT_ERROR(dasm, "Invalid function number: %d", arg);
		else
			print_function_name(dasm, &ain->functions[arg]);
		break;
	case T_DLG:
		if (arg < 0 || arg >= ain->nr_delegates)
			DASM_PRINT_ERROR(dasm, "Invalid delegate number: %d", arg);
		else
			dasm_print_identifier(dasm, ain->delegates[arg].name);
		break;
	case T_STRING:
		if (arg < 0 || arg >= ain->nr_strings)
			DASM_PRINT_ERROR(dasm, "Invalid string number: %d", arg);
		else
			dasm_print_string(dasm, ain->strings[arg]->text);
		break;
	case T_MSG:
		if (arg < 0 || arg >= ain->nr_messages)
			DASM_PRINT_ERROR(dasm, "Invalid message number: %d", arg);
		else
			port_printf(dasm->port, "0x%x ", arg);
		*comment = ain->messages[arg]->text;
		break;
	case T_LOCAL:
		if (dasm->func < 0) {
			DASM_PRINT_ERROR(dasm, "Attempt to access local variable outside of function");
		} else if (arg < 0 || arg >= ain->functions[dasm->func].nr_vars) {
			DASM_PRINT_ERROR(dasm, "Invalid variable number: %d", arg);
			break;
		} else {
			dasm_print_local_variable(dasm, &ain->functions[dasm->func], arg);
		}
		break;
	case T_GLOBAL:
		if (arg < 0 || arg >= ain->nr_globals)
			DASM_PRINT_ERROR(dasm, "Invalid global number: %d", arg);
		else
			dasm_print_identifier(dasm, ain->globals[arg].name);
		break;
	case T_STRUCT:
		if (arg < 0 || arg >= ain->nr_structures)
			DASM_PRINT_ERROR(dasm, "Invalid struct number: %d", arg);
		else
			dasm_print_identifier(dasm, ain->structures[arg].name);
		break;
	case T_SYSCALL:
		if (arg < 0 || arg >= NR_SYSCALLS || !syscalls[arg].name)
			DASM_PRINT_ERROR(dasm, "Invalid/unknown syscall number: %d", arg);
		else
			port_printf(dasm->port, "%s", syscalls[arg].name);
		break;
	case T_HLL:
		if (arg < 0 || arg >= ain->nr_libraries)
			DASM_PRINT_ERROR(dasm, "Invalid HLL library number: %d", arg);
		else
			dasm_print_identifier(dasm, ain->libraries[arg].name);
		break;
	case T_HLLFUNC:
		port_printf(dasm->port, "0x%x", arg);
		break;
	case T_FILE:
		if (!ain->nr_filenames) {
			port_printf(dasm->port, "%d", arg);
			break;
		}
		if (arg < 0 || arg >= ain->nr_filenames)
			DASM_PRINT_ERROR(dasm, "Invalid file number: %d", arg);
		else
			dasm_print_identifier(dasm, ain->filenames[arg]);
		break;
	default:
		port_printf(dasm->port, "<UNKNOWN ARG TYPE: %d>", type);
		break;
	}
}

static void print_arguments(struct dasm_state *dasm, const struct instruction *instr)
{
	if (instr->opcode == CALLHLL) {
		int32_t lib = LittleEndian_getDW(dasm->ain->code, dasm->addr + 2);
		int32_t fun = LittleEndian_getDW(dasm->ain->code, dasm->addr + 6);
		port_printf(dasm->port, " %s ", dasm->ain->libraries[lib].name);
		print_hll_function_name(dasm, &dasm->ain->libraries[lib], fun);
		if (dasm->ain->version >= 11) {
			port_printf(dasm->port, " %d", LittleEndian_getDW(dasm->ain->code, dasm->addr + 10));
		}
		return;
	}
	if (instr->opcode == FUNC) {
		port_putc(dasm->port, ' ');
		port_printf(dasm->port, "%d", dasm->func);
		//ain_dump_function(dasm->out, dasm->ain, &dasm->ain->functions[dasm->func]);
		return;
	}

	const char *comment = NULL;
	for (int i = 0; i < instr->nr_args; i++) {
		port_putc(dasm->port, ' ');
		print_argument(dasm, LittleEndian_getDW(dasm->ain->code, dasm->addr + 2 + i*4), instr->args[i], &comment);
	}
	if (comment) {
		port_printf(dasm->port, "; ");
		dasm_print_string(dasm, comment);
	}
}

static const struct instruction *dasm_get_instruction(struct dasm_state *dasm)
{
	uint16_t opcode = LittleEndian_getW(dasm->ain->code, dasm->addr);
	if (opcode >= NR_OPCODES) {
		if (dasm->flags & DASM_WARN_ON_ERROR) {
			DASM_WARNING(dasm, "Unknown/invalid opcode: %u", opcode);
			goto error;
		} else {
			DASM_ERROR(dasm, "Unknown/invalid opcode: %u", opcode);
		}
	}

	const struct instruction *instr = &instructions[opcode];
	if (dasm->addr + instr->nr_args * 4 >= dasm->ain->code_size) {
		if (dasm->flags & DASM_WARN_ON_ERROR) {
			DASM_WARNING(dasm, "CODE section truncated?");
			goto error;
		} else {
			DASM_ERROR(dasm, "CODE section truncated?");
		}
	}

	return instr;
error:
	// jump to EOF
	dasm->addr = dasm->ain->code_size;
	return &instructions[0];
}

void dasm_init(struct dasm_state *dasm, struct port *port, struct ain *ain, uint32_t flags)
{
	dasm->port = port;
	dasm->ain = ain;
	dasm->flags = flags;
	dasm->addr = 0;
	dasm->func = -1;

	for (int i = 0; i < DASM_FUNC_STACK_SIZE; i++) {
		dasm->func_stack[i] = -1;
	}
}

void dasm_next(struct dasm_state *dasm)
{
	dasm->addr += instruction_width(dasm->instr->opcode);
	dasm->instr = dasm_eof(dasm) ? &instructions[0] : dasm_get_instruction(dasm);
}

enum opcode dasm_peek(struct dasm_state *dasm)
{
	int width = instruction_width(dasm->instr->opcode);
	if (dasm->addr+width >= dasm->ain->code_size)
		return -1;

	return LittleEndian_getW(dasm->ain->code, dasm->addr+width);
}

bool dasm_eof(struct dasm_state *dasm)
{
	return dasm->addr >= dasm->ain->code_size;
}

void dasm_jump(struct dasm_state *dasm, uint32_t addr)
{
	dasm->addr = addr;
	dasm->instr = dasm_eof(dasm) ? &instructions[0] : dasm_get_instruction(dasm);
}

void dasm_reset(struct dasm_state *dasm)
{
	dasm_jump(dasm, 0);
}

dasm_save_t dasm_save(struct dasm_state *dasm)
{
	return (dasm_save_t) { .addr = dasm->addr, .instr = dasm->instr };
}

void dasm_restore(struct dasm_state *dasm, dasm_save_t save)
{
	dasm->addr = save.addr;
	dasm->instr = save.instr;
}

int32_t dasm_arg(struct dasm_state *dasm, unsigned int n)
{
	if ((int)n >= dasm->instr->nr_args)
		return 0;
	return LittleEndian_getDW(dasm->ain->code, dasm->addr + 2 + 4*n);
}

static void print_function_info(struct dasm_state *dasm, int fno)
{
	struct ain_function *f = &dasm->ain->functions[fno];
	port_printf(dasm->port, "\n; ");
	print_function_name(dasm, f);
	port_printf(dasm->port, "\n");
	for (int i = 0; i < f->nr_vars; i++) {
		char *type_sjis = ain_strtype_d(dasm->ain, &f->vars[i].type);
		char *type = conv_output(type_sjis);
		char *name = conv_output(f->vars[i].name);
		port_printf(dasm->port, "; %s %2d: %s : %s\n", i < f->nr_args ? "ARG" : "VAR", i, name, type);
		free(type_sjis);
		free(type);
		free(name);
	}
	char *rtype_sjis = ain_strtype_d(dasm->ain, &f->return_type);
	char *rtype = conv_output(rtype_sjis);
	port_printf(dasm->port, "; RETURN: %s\n", rtype);
	free(rtype_sjis);
	free(rtype);
}

static void dasm_enter_function(struct dasm_state *dasm, int fno)
{
	if (fno < 0 || fno >= dasm->ain->nr_functions) {
		if (dasm->flags & DASM_WARN_ON_ERROR) {
			DASM_WARNING(dasm, "Invalid function number: %d", fno);
			fno = 0;
		} else {
			DASM_ERROR(dasm, "Invalid function number: %d", fno);
		}
	}

	for (int i = 1; i < DASM_FUNC_STACK_SIZE; i++) {
		dasm->func_stack[i] = dasm->func_stack[i-1];
	}
	dasm->func_stack[0] = dasm->func;
	dasm->func = fno;

	print_function_info(dasm, fno);
}

static void dasm_leave_function(struct dasm_state *dasm)
{
	dasm->func = dasm->func_stack[0];
	for (int i = 1; i < DASM_FUNC_STACK_SIZE; i++) {
		dasm->func_stack[i-1] = dasm->func_stack[i];
	}
}

static void print_instruction(struct dasm_state *dasm)
{
	if (dasm->flags & DASM_LABEL_ALL)
		port_printf(dasm->port, "0x%08zX:\t", dasm->addr);

	switch (dasm->instr->opcode) {
	case FUNC:
		dasm_enter_function(dasm, LittleEndian_getDW(dasm->ain->code, dasm->addr + 2));
		break;
	case ENDFUNC:
		dasm_leave_function(dasm);
		break;
	case _EOF:
		break;
	default:
		port_putc(dasm->port, '\t');
		break;
	}

	if (!(dasm->flags & DASM_NO_MACROS) && dasm_print_macro(dasm))
		return;

	port_printf(dasm->port, "%s", dasm->instr->name);
	print_arguments(dasm, dasm->instr);
	port_putc(dasm->port, '\n');
}

static void print_switch_case(struct dasm_state *dasm, struct ain_switch_case *c)
{
	unsigned swi = (unsigned)(c->parent - dasm->ain->switches);
	unsigned ci = (unsigned)(c - c->parent->cases);
	switch (c->parent->case_type) {
	case AIN_SWITCH_INT:
		port_printf(dasm->port, ".CASE %u:%u ", swi, ci);
		port_printf(dasm->port, "%d", c->value);
		break;
	case AIN_SWITCH_STRING:
		port_printf(dasm->port, ".STRCASE %u:%u ", swi, ci);
		dasm_print_string(dasm, dasm->ain->strings[c->value]->text);
		break;
	default:
		WARNING("Unknown switch case type: %d", c->parent->case_type);
		port_printf(dasm->port, "0x%x", c->value);
		break;
	}
	port_putc(dasm->port, '\n');
}

static char *genlabel(size_t addr)
{
	char name[64];
	snprintf(name, 64, "0x%zx", addr);
	return strdup(name);
}

static void generate_labels(struct dasm_state *dasm)
{
	jump_table_init();

	if (!(dasm->flags & DASM_LABEL_ALL)) {
		for (dasm->addr = 0; dasm->addr < dasm->ain->code_size;) {
			const struct instruction *instr = dasm_get_instruction(dasm);
			for (int i = 0; i < instr->nr_args; i++) {
				if (instr->args[i] != T_ADDR)
					continue;
				int32_t arg = LittleEndian_getDW(dasm->ain->code, dasm->addr + 2 + i*4);
				add_label(genlabel(arg), arg);
			}
			dasm->addr += instruction_width(instr->opcode);
		}
	}
	for (int i = 0; i < dasm->ain->nr_switches; i++) {
		add_switch_default(&dasm->ain->switches[i]);
		for (int j = 0; j < dasm->ain->switches[i].nr_cases; j++) {
			add_switch_case(&dasm->ain->switches[i].cases[j]);
		}
	}
}

void ain_disassemble(struct port *port, struct ain *ain, unsigned int flags)
{
	struct dasm_state dasm;
	dasm_init(&dasm, port, ain, flags);

	generate_labels(&dasm);

	for (dasm_reset(&dasm); !dasm_eof(&dasm); dasm_next(&dasm)) {
		jump_list *targets = get_jump_targets(dasm.addr);
		if (targets) {
			for (size_t i = 0; i < kv_size(*targets); i++) {
				struct jump_target *t = kv_A(*targets, i);
				switch (t->type) {
				case JMP_LABEL:
					port_printf(dasm.port, "%s:\n", t->label);
					break;
				case JMP_CASE:
					print_switch_case(&dasm, t->switch_case);
					break;
				case JMP_DEFAULT:
					port_printf(dasm.port, ".DEFAULT %zd\n", t->switch_default - dasm.ain->switches);
					break;
				}
			}
		}
		print_instruction(&dasm);
	}
	//fflush(dasm.out);

	jump_table_fini();
}

bool _ain_disassemble_function(struct port *port, struct ain *ain, int fno, unsigned int flags)
{
	struct dasm_state dasm;
	dasm_init(&dasm, port, ain, flags);

	generate_labels(&dasm);

	uint32_t addr = ain->functions[fno].address - 6;
	for (dasm_jump(&dasm, addr); !dasm_eof(&dasm); dasm_next(&dasm)) {
		jump_list *targets = get_jump_targets(dasm.addr);
		if (targets) {
			for (size_t i = 0; i < kv_size(*targets); i++) {
				struct jump_target *t = kv_A(*targets, i);
				switch (t->type) {
				case JMP_LABEL:
					port_printf(dasm.port, "%s:\n", t->label);
					break;
				case JMP_CASE:
					print_switch_case(&dasm, t->switch_case);
					break;
				case JMP_DEFAULT:
					port_printf(dasm.port, ".DEFAULT %zd\n", t->switch_default - dasm.ain->switches);
					break;
				}
			}
		}
		// XXX: functions don't always end with ENDFUNC
		if (dasm.instr->opcode == FUNC) {
			int n = dasm_arg(&dasm, 0);
			if (n != fno && !strstr(ain->functions[n].name, "<lambda"))
				break;
		}
		print_instruction(&dasm);
		//fflush(dasm.out);
		if (dasm.instr->opcode == ENDFUNC && dasm_arg(&dasm, 0) == fno)
			break;
	}
	//fflush(dasm.out);
	jump_table_fini();
	return true;
}

bool ain_disassemble_function(struct port *port, struct ain *ain, char *_name, unsigned int flags)
{
	char *name = conv_utf8_input(_name);
	int fno = ain_get_function(ain, name);
	free(name);
	if (fno < 0)
		return false;
	return _ain_disassemble_function(port, ain, fno, flags);
}
