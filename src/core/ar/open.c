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
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <libgen.h>
#include "system4.h"
#include "system4/aar.h"
#include "system4/afa.h"
#include "system4/ald.h"
#include "system4/alk.h"
#include "system4/dlf.h"
#include "system4/file.h"
#include "system4/flat.h"
#include "alice.h"
#include "alice/ar.h"

struct archive *open_ald_archive(const char *path, int *error, char *(*conv)(const char*))
{
	int count = 0;
	char *dir_name = path_dirname(path);
	char *base_name = path_basename(path);
	char *ald_filenames[ALD_FILEMAX];
	int prefix_len = strlen(base_name) - 5;
	if (prefix_len <= 0)
		return NULL;

	memset(ald_filenames, 0, sizeof(char*) * ALD_FILEMAX);

	DIR *dir;
	struct dirent *d;
	char filepath[PATH_MAX];

	if (!(dir = opendir(dir_name))) {
		*error = ARCHIVE_FILE_ERROR;
		return NULL;
	}

	while ((d = readdir(dir))) {
		int len = strlen(d->d_name);
		if (len < prefix_len + 5 || strcasecmp(d->d_name+len-4, ".ald"))
			continue;
		if (strncasecmp(d->d_name, base_name, prefix_len))
			continue;

		int dno = toupper(*(d->d_name+len-5)) - 'A';
		if (dno < 0 || dno >= ALD_FILEMAX)
			continue;

		snprintf(filepath, PATH_MAX-1, "%s/%s", dir_name, d->d_name);
		ald_filenames[dno] = strdup(filepath);
		count = max(count, dno+1);
	}

	struct archive *ar = ald_open_conv(ald_filenames, count, ARCHIVE_MMAP, error, conv);

	for (int i = 0; i < ALD_FILEMAX; i++) {
		free(ald_filenames[i]);
	}

	return ar;
}

struct archive *open_archive(const char *path, enum archive_type *type, int *error)
{
	size_t len = strlen(path);
	if (len < 4)
		goto err;

	const char *ext = path + len - 4;
	if (!strcasecmp(ext, ".ald")) {
		*type = AR_ALD;
		return open_ald_archive(path, error, strdup);
	} else if (!strcasecmp(ext, ".afa")) {
		*type = AR_AFA;
		return (struct archive*)afa_open(path, ARCHIVE_MMAP, error);
	} else if (!strcasecmp(ext, "flat")) {
		*type = AR_FLAT;
		return (struct archive*)flat_open_file(path, 0, error);
	} else if (!strcasecmp(ext, ".dlf")) {
		*type = AR_DLF;
		return (struct archive*)dlf_open(path, ARCHIVE_MMAP, error);
	} else if (!strcasecmp(ext, ".alk")) {
		*type = AR_ALK;
		return (struct archive*)alk_open(path, ARCHIVE_MMAP, error);
	} else if (!strcasecmp(ext, ".red")) {
		*type = AR_AAR;
		return (struct archive*)aar_open(path, ARCHIVE_MMAP, error);
	}
	// TODO: try to use file magic

err:
	WARNING("Couldn't determine archive type for '%s'", path);
	return NULL;
}
