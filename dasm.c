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
#include "system4.h"
#include "system4/ain.h"
#include "system4/instructions.h"
#include "little_endian.h"
#include "dasm.h"

static const struct instruction *dasm_get_instruction(struct dasm_state *dasm)
{
	uint16_t opcode = LittleEndian_getW(dasm->ain->code, dasm->addr);
	if (opcode >= NR_OPCODES)
		DASM_ERROR(dasm, "Unknown/invalid opcode: %u", opcode);

	const struct instruction *instr = &instructions[opcode];
	if (dasm->addr + instr->nr_args * 4 >= dasm->ain->code_size)
		DASM_ERROR(dasm, "CODE section truncated?");

	return instr;
}

void dasm_init(struct dasm_state *dasm, FILE *out, struct ain *ain, uint32_t flags)
{
	dasm->out = out;
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

void dasm_reset(struct dasm_state *dasm)
{
	dasm->addr = 0;
	dasm->instr = dasm_eof(dasm) ? &instructions[0] : dasm_get_instruction(dasm);
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

