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

#ifndef ALICE_CLI_CLI_H
#define ALICE_CLI_CLI_H

#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>

#define USAGE_ERROR(cmd, msg, ...) (print_usage(cmd), ALICE_ERROR(msg, ##__VA_ARGS__))

struct alice_option {
	char *name;
	char short_opt;
	const char *description;
	int has_arg;
	int val;
};

struct command {
	char *name;
	char *usage;
	char *description;
	bool hidden;
	struct command *parent;
	struct command *commands[16];
	int (*fun)(int, char*[]);
	struct alice_option options[32];
};

void print_usage(struct command *cmd);
int alice_getopt(int argc, char *argv[], struct command *cmd);
FILE *alice_open_output_file(const char *path);

extern struct command cmd_acx_dump;
extern struct command cmd_acx_build;
extern struct command cmd_ain_compare;
extern struct command cmd_ain_dump;
extern struct command cmd_ain_edit;
extern struct command cmd_ar_extract;
extern struct command cmd_ar_list;
extern struct command cmd_ar_pack;
extern struct command cmd_asd_dump;
extern struct command cmd_asd_build;
extern struct command cmd_cg_convert;
extern struct command cmd_cg_thumbnail;
extern struct command cmd_ex_build;
extern struct command cmd_ex_compare;
extern struct command cmd_ex_dump;
extern struct command cmd_ex_edit;
extern struct command cmd_flat_build;
extern struct command cmd_flat_extract;
extern struct command cmd_fnl_dump;
extern struct command cmd_project_build;
extern struct command cmd_project_init;

extern struct command cmd_alice;
extern struct command cmd_acx;
extern struct command cmd_ain;
extern struct command cmd_ar;
extern struct command cmd_asd;
extern struct command cmd_cg;
extern struct command cmd_ex;
extern struct command cmd_flat;
extern struct command cmd_fnl;
extern struct command cmd_project;

#endif /* ALICE_CLI_CLI_H */
