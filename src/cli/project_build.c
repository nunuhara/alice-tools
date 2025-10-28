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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "system4.h"
#include "system4/file.h"
#include "alice.h"
#include "alice/project.h"
#include "cli.h"

static int command_project_build(int argc, char *argv[])
{
	set_input_encoding("UTF-8");
	set_output_encoding("CP932");

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_project_build);
		if (c == -1)
			break;
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		USAGE_ERROR(&cmd_project_build, "Wrong number of arguments");

	pje_build(argv[0]);
	return 0;
}

struct command cmd_project_build = {
	.name = "build",
	.usage = "[options...] <input-file>",
	.description = "Build a .pje project",
	.parent = &cmd_project,
	.fun = command_project_build,
	.options = {
		{ 0 }
	}
};
