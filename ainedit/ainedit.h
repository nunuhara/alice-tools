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

#ifndef AINEDIT_AINEDIT_H
#define AINEDIT_AINEDIT_H

#include <stdint.h>
#include "system4/ain.h"
#include "system4/instructions.h"

enum {
	ASM_RAW        = 1,
};

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
	PO_LOCALDEC,
	PO_LOCALDEC2,
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

const struct instruction asm_pseudo_ops[NR_PSEUDO_OPS - PSEUDO_OP_OFFSET];

// pje.c
void pje_build(const char *path, int ain_version);

// asm.c
void asm_assemble_jam(const char *filename, struct ain *ain, uint32_t flags);
const struct instruction *asm_get_instruction(const char *name);
const_pure int32_t asm_instruction_width(int opcode);

// json.c
void read_declarations(const char *filename, struct ain *ain);

// repack.c
void ain_write(const char *filename, struct ain *ain);

// text.c
void read_text(const char *filename, struct ain *ain);

// transcode.c
void ain_transcode(struct ain *ain);

char *encode_text(const char *str);
char *encode_text_for_print(const char *str);

#endif /* AINEDIT_AINEDIT_H */
