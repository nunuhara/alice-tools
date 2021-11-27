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
#include "system4.h"
#include "system4/archive.h"
#include "alice.h"
#include "alice/ar.h"
#include "cli.h"

static void list_all_iter(struct archive_data *data, possibly_unused void *_)
{
	char *name = conv_utf8(data->name);
	printf("%d: %s\n", data->no, name);
	free(name);
}

int command_ar_list(int argc, char *argv[])
{
	while (1) {
		int c = alice_getopt(argc, argv, &cmd_ar_extract);
		if (c == -1)
			break;
	}

	argc -= optind;
	argv += optind;

	// check argument count
	if (argc != 1) {
		USAGE_ERROR(&cmd_ar_extract, "Wrong number of arguments");
	}

	// open archive
	struct archive *ar;
	enum archive_type type;
	int error;
	ar = open_archive(argv[0], &type, &error);
	if (!ar) {
		ERROR("Opening archive: %s", archive_strerror(error));
	}

	archive_for_each(ar, list_all_iter, NULL);

	archive_free(ar);
	return 0;
}

struct command cmd_ar_list = {
	.name = "list",
	.usage = "[options...] <input-file>",
	.description = "List the contents of an archive file",
	.parent = &cmd_ar,
	.fun = command_ar_list,
	.options = {
		{ 0 }
	}
};
