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

#ifndef ALICE_AIN_H
#define ALICE_AIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "system4/instructions.h"

struct ain;
struct ain_function;
struct instruction;
struct port;

enum {
	ASM_RAW         = 1,
	ASM_NO_VALIDATE = 2,
};

enum {
	DASM_NO_IDENTIFIERS = 1, // don't print identifiers
	DASM_LABEL_ALL = 2,      // label every bytecode instruction
	DASM_NO_MACROS = 4,      // don't output macros
	DASM_WARN_ON_ERROR = 8,
};

#define DASM_ERROR(dasm, fmt, ...) ERROR("At 0x%x: " fmt, dasm->addr, ##__VA_ARGS__)
#define DASM_WARNING(dasm, fmt, ...) WARNING("At 0x%x: " fmt, dasm->addr, ##__VA_ARGS__)

#define DASM_FUNC_STACK_SIZE 16

struct dasm_state {
	struct ain *ain;
	uint32_t flags;
	struct port *port;
	size_t addr;
	int func;
	int func_stack[DASM_FUNC_STACK_SIZE];
	const struct instruction *instr;
};

typedef struct {
	size_t addr;
	const struct instruction *instr;
} dasm_save_t;

// asm.c
void ain_assemble_jam(const char *filename, struct ain *ain, uint32_t flags);
void ain_append_jam(const char *filename, struct ain *ain, int32_t flags);
void ain_inject_jam(const char *filename, struct ain *ain, char *function, unsigned offset, int32_t flags);

// dasm.c
void dasm_init(struct dasm_state *dasm, struct port *port, struct ain *ain, uint32_t flags);
void dasm_next(struct dasm_state *dasm);
void dasm_jump(struct dasm_state *dasm, uint32_t addr);
enum opcode dasm_peek(struct dasm_state *dasm);
bool dasm_eof(struct dasm_state *dasm);
void dasm_reset(struct dasm_state *dasm);
dasm_save_t dasm_save(struct dasm_state *dasm);
void dasm_restore(struct dasm_state *dasm, dasm_save_t save);
int32_t dasm_arg(struct dasm_state *dasm, unsigned int n);
bool dasm_is_jump_target(struct dasm_state *dasm);
bool dasm_print_macro(struct dasm_state *dasm);
void dasm_print_identifier(struct dasm_state *dasm, const char *str);
void dasm_print_local_variable(struct dasm_state *dasm, struct ain_function *func, int varno);
void dasm_print_string(struct dasm_state *dasm, const char *str);
void ain_disassemble(struct port *port, struct ain *ain, unsigned int flags);
bool _ain_disassemble_function(struct port *port, struct ain *ain, int fno, unsigned int flags);
bool ain_disassemble_function(struct port *port, struct ain *ain, char *name, unsigned int flags);

// dump.c
void ain_dump_function(struct port *port, struct ain *ain, struct ain_function *f);
void ain_dump_global(struct port *port, struct ain *ain, int i);
void ain_dump_structure(struct port *port, struct ain *ain, int i);
void ain_dump_text(struct port *port, struct ain *ain);
void ain_dump_library(struct port *port, struct ain *ain, int lib);
void ain_dump_library_stub(struct port *port, struct ain_library *lib);
void ain_dump_functype(struct port *port, struct ain *ain, int i, bool delegate);
void ain_dump_enum(struct port *port, struct ain *ain, int i);

// guess_filenames.c
void ain_guess_filenames(struct ain *ain);

// json_dump.c
void ain_dump_json(FILE *out, struct ain *ain);

// json_read.c
void ain_read_json(const char *filename, struct ain *ain);

// repack.c
void ain_write(const char *filename, struct ain *ain);

// text.c
void ain_read_text(const char *filename, struct ain *ain);

// transcode.c
void ain_transcode(struct ain *ain);

#endif /* ALICE_AIN_H */
