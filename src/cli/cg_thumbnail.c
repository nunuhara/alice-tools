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
#include "system4/cg.h"
#include "system4/file.h"
#include "system4/string.h"
#include "alice.h"
#include "cli.h"

struct cg *scale_cg_bicubic(struct cg *in, float scale);

enum {
	LOPT_OUTPUT = 256,
	LOPT_SIZE,
};

static int command_cg_thumbnail(int argc, char *argv[])
{
	char *output_file = NULL;
	int size = 256;

	while (1) {
		int c = alice_getopt(argc, argv, &cmd_cg_thumbnail);
		if (c == -1)
			break;
		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case 's':
		case LOPT_SIZE:
			size = atoi(optarg);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		USAGE_ERROR(&cmd_cg_thumbnail, "Wrong number of arguments");
	if (size < 16 || size > 4096)
		USAGE_ERROR(&cmd_cg_thumbnail, "Size out of range (allowed range is [16-4096])");

	struct cg *in = cg_load_file(argv[0]);
	if (!in)
		ALICE_ERROR("Failed to load input CG: %s", argv[0]);

	// scale/write output CG
	float scale = (float)size / (float)max(in->metrics.w, in->metrics.h);
	struct cg *out = scale_cg_bicubic(in, scale);
	// TODO: output format other than png?
	FILE *f = checked_fopen(output_file ? output_file : "out.png", "wb");
	if (!cg_write(out, ALCG_PNG, f))
		ALICE_ERROR("cg_write failed");

	cg_free(in);
	cg_free(out);
	fclose(f);
	return 0;
}

struct command cmd_cg_thumbnail = {
	.name = "thumbnail",
	.usage = "[options...] <input-file>",
	.description = "Create a thumbnail from a CG",
	.parent = &cmd_cg,
	.fun = command_cg_thumbnail,
	.options = {
		{ "output", 'o', "Specify output file (default 'out.png')", required_argument, LOPT_OUTPUT },
		{ "size",   's', "Specify output size (default 256)", required_argument, LOPT_SIZE },
		{ 0 }
	}
};
