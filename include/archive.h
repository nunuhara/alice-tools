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

#ifndef ALICE_ARCHIVE_H_
#define ALICE_ARCHIVE_H_

#include "system4/archive.h"

enum archive_type {
	AR_ALD,
	AR_AFA,
	AR_AFA3,
	AR_FLAT,
	AR_DLF,
	AR_ALK,
};

struct archive *open_archive(const char *path, enum archive_type *type, int *error);
struct afa3_archive *afa3_open(const char *file, int flags, int *error);

#endif /* ALICE_ARCHIVE_H_ */
