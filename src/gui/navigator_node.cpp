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

#include <QFileInfo>
#include "navigator_node.hpp"

extern "C" {
#include "system4/archive.h"
#include "system4/cg.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/ex.h"
#include "alice/port.h"
}

static QVector<FileFormat> getSupportedConversionFormats(FileFormat from)
{
	switch (from) {
	case FileFormat::NONE:
	case FileFormat::AIN:
	case FileFormat::ALD:
	case FileFormat::AFA:
	case FileFormat::ALK:
	case FileFormat::ACX:
		return QVector<FileFormat>();
	case FileFormat::EX:
	case FileFormat::TXTEX:
		return QVector({FileFormat::EX, FileFormat::TXTEX});
	case FileFormat::PNG:
	case FileFormat::WEBP:
	case FileFormat::QNT:
	case FileFormat::AJP:
	case FileFormat::DCF:
		return QVector({FileFormat::PNG, FileFormat::WEBP, FileFormat::QNT});
	case FileFormat::JAF:
	case FileFormat::JAM:
		return QVector({from});
	}
	return QVector<FileFormat>();
}

/*
 * Get the list of valid export formats.
 */
QVector<FileFormat> NavigatorNode::getSupportedFormats() const
{
	switch (type) {
	case RootNode:
		return QVector<FileFormat>();
	case ClassNode:
		return QVector({FileFormat::JAF});
	case FunctionNode:
		return QVector({FileFormat::JAM});
	case ExStringKeyValueNode:
	case ExIntKeyValueNode:
	case ExRowNode:
		return QVector({FileFormat::TXTEX});
	case FileNode:
		return getSupportedConversionFormats(
				extensionToFileFormat(
					QFileInfo(ar.file->name).suffix()));
	}
	return QVector<FileFormat>();
}

static bool convertFormat(struct port *port, uint8_t *data, size_t size, FileFormat from, FileFormat to)
{
	if (from == to) {
		return port_write_bytes(port, data, size);
	}

	struct cg *cg;
	enum cg_type cg_type;
	switch (from) {
	case FileFormat::PNG:
	case FileFormat::WEBP:
	case FileFormat::QNT:
	case FileFormat::AJP:
	case FileFormat::DCF:
		switch (to) {
		case FileFormat::PNG:  cg_type = ALCG_PNG;  break;
		case FileFormat::WEBP: cg_type = ALCG_WEBP; break;
		case FileFormat::QNT:  cg_type = ALCG_QNT;  break;
		default: return false;
		}
		// FIXME: cg_write can only write to files...
		if (port->type != PORT_TYPE_FILE)
			return false;
		if (!(cg = cg_load_buffer(data, size)))
			return false;
		cg_write(cg, cg_type, port->file);
		cg_free(cg);
		return true;
	case FileFormat::EX:
		if (to != FileFormat::TXTEX)
			return false;
		// TODO
		return false;
	case FileFormat::TXTEX:
		if (to != FileFormat::EX)
			return false;
		// TODO
		return false;
	case FileFormat::NONE:
	case FileFormat::AIN:
	case FileFormat::JAF:
	case FileFormat::JAM:
	case FileFormat::ALD:
	case FileFormat::AFA:
	case FileFormat::ALK:
	case FileFormat::ACX:
		return false;;
	}
	return false;
}

bool NavigatorNode::write(struct port *port, FileFormat format) const
{
	bool r;
	switch (type) {
	case RootNode:
		return false;
	case ClassNode:
		if (format != FileFormat::JAF)
			return false;
		set_encodings("UTF-8", "UTF-8");
		ain_dump_structure(port, ainItem.ainFile, ainItem.i);
		return true;
	case FunctionNode:
		if (format != FileFormat::JAM)
			return false;
		set_encodings("UTF-8", "UTF-8");
		_ain_disassemble_function(port, ainItem.ainFile, ainItem.i, 0);
		return true;
	case ExStringKeyValueNode:
	case ExIntKeyValueNode:
		if (format != FileFormat::TXTEX)
			return false;
		set_encodings("UTF-8", "UTF-8");
		if (type == ExStringKeyValueNode)
			ex_dump_key_value(port, exKV.key.s, exKV.value);
		else
			ex_dump_value(port, exKV.value);
		return true;
	case ExRowNode:
		if (format != FileFormat::TXTEX)
			return false;
		set_encodings("UTF-8", "UTF-8");
		ex_dump_table_row(port, exRow.t, exRow.i);
		return true;
	case FileNode:
		if (!archive_load_file(ar.file))
			return false;
		r = convertFormat(port, ar.file->data, ar.file->size,
				extensionToFileFormat(QFileInfo(ar.file->name).suffix()),
				format);
		archive_release_file(ar.file);
		return r;
	}
	return false;
}
