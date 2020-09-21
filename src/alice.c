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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iconv.h>
#include "system4.h"
#include "alice.h"

struct command cmd_alice;
struct command cmd_acx;
struct command cmd_ain;
struct command cmd_ar;
struct command cmd_ex;
struct command cmd_fnl;

struct command cmd_acx = {
	.name = "acx",
	.usage = "<command> ...",
	.description = "Tools for extracting and editing .acx files",
	.parent = &cmd_alice,
	.commands = {
		&cmd_acx_dump,
		&cmd_acx_build,
		NULL
	}
};

struct command cmd_ain = {
	.name = "ain",
	.usage = "<command> ...",
	.description = "Tools for extracting and editing .ain files",
	.parent = &cmd_alice,
	.commands = {
		&cmd_ain_dump,
		&cmd_ain_edit,
		&cmd_ain_compare,
		NULL
	}
};

struct command cmd_ar = {
	.name = "ar",
	.usage = "<command> ...",
	.description = "Tools for extracting and editing AliceSoft archive files",
	.parent = &cmd_alice,
	.commands = {
		&cmd_ar_extract,
		&cmd_ar_list,
		NULL
	}
};

struct command cmd_ex = {
	.name = "ex",
	.usage = "<command> ...",
	.description = "Tools for extracting and editing .ex files",
	.parent = &cmd_alice,
	.commands = {
		&cmd_ex_dump,
		&cmd_ex_build,
		&cmd_ex_compare,
		NULL
	}
};

struct command cmd_fnl = {
	.name = "fnl",
	.usage = "<command> ...",
	.description = "Tools for extracting and editing .fnl font libraries",
	.hidden = true,
	.parent = &cmd_alice,
	.commands = {
		&cmd_fnl_dump,
	}
};

struct command cmd_alice = {
	.name = "alice",
	.usage = "<command> ...",
	.description = "Toolkit for extracting and editing AliceSoft file formats",
	.parent = NULL,
	.commands = {
		&cmd_acx,
		&cmd_ain,
		&cmd_ar,
		&cmd_ex,
		&cmd_fnl,
		NULL
	}
};

FILE *alice_open_output_file(const char *path)
{
	if (!path)
		return stdout;
	FILE *out = fopen(path, "wb");
	if (!out)
		ALICE_ERROR("fopen: %s", strerror(errno));
	return out;
}

void print_usage(struct command *cmd)
{
	// flatten command path
	int nr_commands = 0;
	const char *cmd_path[16];
	for (struct command *p = cmd; p; p = p->parent) {
		cmd_path[nr_commands++] = p->name;
	}

	printf("Usage: ");
	for (int i = nr_commands - 1; i >= 0; i--) {
		printf("%s ", cmd_path[i]);
	}
	printf("%s\n", cmd->usage);
	printf("    %s\n", cmd->description);

	if (cmd->fun) {
		// calculate column width
		size_t width = 0;
		for (struct alice_option *opt = cmd->options; opt->name; opt++) {
			size_t this_width = strlen(opt->name) + 2;
			if (opt->short_opt)
				this_width += 3;
			if (opt->has_arg == required_argument)
				this_width += 6;
			width = max(width, this_width);
		}

		printf("Command options:\n");
		for (struct alice_option *opt = cmd->options; opt->name; opt++) {
			size_t written = 0;
			printf("    ");
			if (opt->short_opt) {
				written += printf("-%c,", opt->short_opt);
			}
			written += printf("--%s", opt->name);
			if (opt->has_arg == required_argument) {
				written += printf(" <arg>");
			}
			printf("%*s    %s\n", (int)(width - written), "", opt->description);
		}
		printf("Common options:\n");
		printf("    -h,--help                  Print this message and exit\n");
		printf("    --input-encoding <arg>     Specify the input encoding\n");
		printf("    --output-encoding <arg>    Specify the output encoding\n");
	} else {
		// calculate column width
		size_t width = 0;
		for (int i = 0; cmd->commands[i]; i++) {
			if (cmd->commands[i]->hidden)
				continue;
			width = max(width, strlen(cmd->commands[i]->name));
		}

		printf("Commands:\n");
		for (int i = 0; cmd->commands[i]; i++) {
			if (cmd->commands[i]->hidden)
				continue;
			printf("    ");
			for (int j = nr_commands-1; j >= 0; j--) {
				printf("%s ", cmd_path[j]);
			}
			printf("%-*s    %s\n", (int)width, cmd->commands[i]->name, cmd->commands[i]->description);
		}
	}
}

enum {
	LOPT_HELP = -2,
	LOPT_INPUT_ENCODING = -3,
	LOPT_OUTPUT_ENCODING = -4,
};

int alice_getopt(int argc, char *argv[], struct command *cmd)
{
	static struct option long_opts[32];
	static char short_opts[64];
	static bool initialized = false;

	// process options on first invocation
	if (!initialized) {
		int nr_opts = 0;
		int nr_short_opts = 0;

		for (nr_opts = 0; cmd->options[nr_opts].name; nr_opts++) {
			long_opts[nr_opts] = (struct option) {
				.name = cmd->options[nr_opts].name,
				.has_arg = cmd->options[nr_opts].has_arg,
				.flag = NULL,
				.val = cmd->options[nr_opts].val
			};
			if (cmd->options[nr_opts].short_opt) {
				short_opts[nr_short_opts++] = cmd->options[nr_opts].short_opt;
				if (cmd->options[nr_opts].has_arg == required_argument) {
					short_opts[nr_short_opts++] = ':';
				}
			}
		}
		long_opts[nr_opts++] = (struct option) { "help",            no_argument,       NULL, LOPT_HELP };
		long_opts[nr_opts++] = (struct option) { "input-encoding",  required_argument, NULL, LOPT_INPUT_ENCODING };
		long_opts[nr_opts++] = (struct option) { "output-encoding", required_argument, NULL, LOPT_OUTPUT_ENCODING };
		long_opts[nr_opts] = (struct option) { 0, 0, 0, 0 };
		short_opts[nr_short_opts++] = 'h';
		short_opts[nr_short_opts] = '\0';
		initialized = true;
	}

	int option_index = 0;
	int c = getopt_long(argc, argv, short_opts, long_opts, &option_index);
	switch (c) {
	case 'h':
	case LOPT_HELP:
		print_usage(cmd);
		exit(0);
	case LOPT_INPUT_ENCODING:
		set_input_encoding(optarg);
		break;
	case LOPT_OUTPUT_ENCODING:
		set_output_encoding(optarg);
		break;
	case '?':
		USAGE_ERROR(cmd, "Unrecognized command line argument");
	}
	return c;
}

static int process_command(struct command *cmd, int argc, char *argv[])
{
	if (cmd->fun) {
		if (argc < 2) {
			print_usage(cmd);
			exit(0);
		}
		return cmd->fun(argc, argv);
	}

	if (argc < 2) {
		print_usage(cmd);
		exit(0);
	}

	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		print_usage(cmd);
		exit(0);
	}

	for (int i = 0; cmd->commands[i]; i++) {
		if (!strcmp(argv[1], cmd->commands[i]->name)) {
			return process_command(cmd->commands[i], argc-1, argv+1);
		}
	}

	USAGE_ERROR(cmd, "Unrecognized command: %s", argv[1]);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		print_usage(&cmd_alice);
		exit(0);
	}

	return process_command(&cmd_alice, argc, argv);
}
