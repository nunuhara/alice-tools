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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "system4.h"
#include "system4/buffer.h"
#include "system4/file.h"
#include "alice.h"
#include "alice/port.h"

void port_buffer_init(struct port *port)
{
	port->type = PORT_TYPE_BUFFER;
	buffer_init(&port->buffer, NULL, 0);
}

void port_file_init(struct port *port, FILE *f)
{
	port->type = PORT_TYPE_FILE;
	port->file = f;
	port->need_close = false;
}

bool port_file_open(struct port *port, const char *path)
{
	port->type = PORT_TYPE_FILE;
	port->file = file_open_utf8(path, "wb");
	port->need_close = true;
	return !!port->file;
}

uint8_t *port_buffer_get(struct port *port, size_t *size_out)
{
	if (size_out)
		*size_out = port->buffer.size;
	buffer_write_int8(&port->buffer, '\0');
	uint8_t *data = port->buffer.buf;
	buffer_init(&port->buffer, NULL, 0);
	return data;
}

void port_close(struct port *port)
{
	if (port->type == PORT_TYPE_BUFFER) {
		free(port->buffer.buf);
		buffer_init(&port->buffer, NULL, 0);
	} else if (port->type == PORT_TYPE_FILE) {
		if (port->need_close) {
			fclose(port->file);
		} else {
			fflush(port->file);
		}
		port->file = NULL;
		port->need_close = false;
	}
}

void port_printf(struct port *port, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (port->type == PORT_TYPE_BUFFER) {
		char tmp[4096];
		int n = vsnprintf(tmp, 4096, fmt, ap);
		buffer_write_bytes(&port->buffer, (uint8_t*)tmp, n);
	} else if (port->type == PORT_TYPE_FILE) {
		vfprintf(port->file, fmt, ap);
	}

	va_end(ap);
}

void port_putc(struct port *port, char c)
{
	if (port->type == PORT_TYPE_BUFFER) {
		buffer_write_int8(&port->buffer, (uint8_t)c);
	} else if (port->type == PORT_TYPE_FILE) {
		fputc(c, port->file);
	}
}

bool port_write_bytes(struct port *port, uint8_t *data, size_t size)
{
	if (port->type == PORT_TYPE_BUFFER) {
		buffer_write_bytes(&port->buffer, data, size);
		return true;
	} else if (port->type == PORT_TYPE_FILE) {
		return fwrite(data, size, 1, port->file) == 1;
	}
	return false;
}
