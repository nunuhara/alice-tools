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

#ifndef ALICE_PORT_H
#define ALICE_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "system4/buffer.h"

/*
 * A port can be backed by either an in-memory buffer or a file handle.
 */
enum port_type {
	PORT_TYPE_BUFFER,
	PORT_TYPE_FILE,
};

struct port {
	enum port_type type;
	union {
		struct {
			FILE *file;
			bool need_close;
		};
		struct buffer buffer;
	};
};

/*
 * Initialize a buffer (memory) port.
 */
void port_buffer_init(struct port *port);

/*
 * Initialize a file port with an open file.
 * The caller is responsible for closing the file afterwards.
 */
void port_file_init(struct port *port, FILE *f);

/*
 * Initialize a file port with a path name.
 * Returns true if the file was opened; otherwise returns false.
 */
bool port_file_open(struct port *port, const char *path);

/*
 * Get the data from a buffer port.
 * This clears the buffer. The caller is responsible for freeing the returned
 * memory.
 */
uint8_t *port_buffer_get(struct port *port, size_t *size_out);

/*
 * Close a port.
 * This frees any data remaining in a buffer port, and closes the file handle
 * associated with a file port (if it was opened with `port_file_open`).
 */
void port_close(struct port *port);

/*
 * Write formatted data to a port.
 */
void port_printf(struct port *port, const char *fmt, ...);

/*
 * Write a character to a port.
 */
void port_putc(struct port *port, char c);

/*
 * Write a series of bytes to a port.
 */
bool port_write_bytes(struct port *port, uint8_t *data, size_t size);

#endif /* ALICE_PORT_H */
