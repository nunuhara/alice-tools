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

#ifndef CORE_AIN_ASM_H
#define CORE_AIN_ASM_H

#include <stdint.h>
#include <stdio.h>
#include "kvec.h"
#include "khash.h"
#include "system4.h"
#include "system4/ain.h"
#include "system4/instructions.h"

#define PSEUDO_OP_OFFSET 0xF000
enum asm_pseudo_opcode {
	PO_CASE = PSEUDO_OP_OFFSET,
	PO_STRCASE,
	PO_DEFAULT,
	PO_SETSTR,
	PO_SETMSG,
	PO_MSG,
	PO_LOCALREF,
	PO_GLOBALREF,
	PO_LOCALREFREF,
	PO_GLOBALREFREF,
	PO_LOCALINC,
	PO_LOCALINC2,
	PO_LOCALINC3,
	PO_LOCALDEC,
	PO_LOCALDEC2,
	PO_LOCALDEC3,
	PO_LOCALPLUSA,
	PO_LOCALMINUSA,
	PO_LOCALASSIGN,
	PO_LOCALASSIGN2,
	PO_F_LOCALASSIGN,
	PO_STACK_LOCALASSIGN,
	PO_S_LOCALASSIGN,
	PO_LOCALDELETE,
	PO_LOCALCREATE,
	PO_GLOBALINC,
	PO_GLOBALDEC,
	PO_GLOBALASSIGN,
	PO_F_GLOBALASSIGN,
	PO_STRUCTREF,
	PO_STRUCTREFREF,
	PO_STRUCTINC,
	PO_STRUCTDEC,
	PO_STRUCTASSIGN,
	PO_F_STRUCTASSIGN,
	PO_PUSHVMETHOD,
	NR_PSEUDO_OPS
};

extern struct instruction asm_pseudo_ops[NR_PSEUDO_OPS - PSEUDO_OP_OFFSET];

kv_decl(parse_instruction_list, struct parse_instruction*);
kv_decl(parse_argument_list, struct string*);
kv_decl(pointer_list, uint32_t*);

struct parse_instruction {
	uint16_t opcode;
	parse_argument_list *args;
};

extern FILE *asm_in;
extern unsigned long asm_line;
extern parse_instruction_list *parsed_code;
extern uint32_t asm_instr_ptr;

KHASH_MAP_INIT_STR(label_table, uint32_t);
extern khash_t(label_table) *label_table;

int asm_parse(void);

const struct instruction *asm_get_instruction(const char *name);
const_pure int32_t asm_instruction_width(int opcode);

#endif /* CORE_AIN_ASM_H */
