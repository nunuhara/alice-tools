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

#ifndef AINDUMP_DASM_H
#define AINDUMP_DASM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct ain_function;

#define DASM_ERROR(dasm, fmt, ...) ERROR("At 0x%x: " fmt, dasm->addr, ##__VA_ARGS__)
#define DASM_WARNING(dasm, fmt, ...) WARNING("At 0x%x: " fmt, dasm->addr, ##__VA_ARGS__)

#define DASM_FUNC_STACK_SIZE 16

struct dasm_state {
	struct ain *ain;
	uint32_t flags;
	FILE *out;
	size_t addr;
	int func;
	int func_stack[DASM_FUNC_STACK_SIZE];
	const struct instruction *instr;
};

typedef struct {
	size_t addr;
	const struct instruction *instr;
} dasm_save_t;

void dasm_init(struct dasm_state *dasm, FILE *out, struct ain *ain, uint32_t flags);
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

#endif /* AINDUMP_DASM_H */
