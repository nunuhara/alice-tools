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
#include "galice.hpp"
#include "navigator_node.hpp"

extern "C" {
#include "system4/archive.h"
#include "system4/cg.h"
#include "system4/ex.h"
#include "system4/string.h"
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
	case FileFormat::ACX:
	case FileFormat::AAR:
	case FileFormat::AFA:
	case FileFormat::ALD:
	case FileFormat::ALK:
	case FileFormat::DLF:
	case FileFormat::FLAT:
		return QVector<FileFormat>();
	case FileFormat::EX:
	case FileFormat::TXTEX:
		return QVector<FileFormat>({FileFormat::EX, FileFormat::TXTEX});
	case FileFormat::PNG:
	case FileFormat::WEBP:
	case FileFormat::QNT:
		return QVector<FileFormat>({FileFormat::PNG, FileFormat::WEBP, FileFormat::QNT});
	case FileFormat::AJP:
		return QVector<FileFormat>({FileFormat::AJP, FileFormat::PNG, FileFormat::WEBP, FileFormat::QNT});
	case FileFormat::DCF:
		return QVector<FileFormat>({FileFormat::DCF, FileFormat::PNG, FileFormat::WEBP, FileFormat::QNT});
	case FileFormat::PCF:
		return QVector<FileFormat>({FileFormat::PCF, FileFormat::PNG, FileFormat::WEBP, FileFormat::QNT});
	case FileFormat::JAF:
	case FileFormat::JAM:
		return QVector<FileFormat>({from});
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
	case BranchNode:
		return QVector<FileFormat>();
	case ClassNode:
	case EnumNode:
	case GlobalNode:
	case FuncTypeNode:
	case DelegateNode:
	case LibraryNode:
		return QVector<FileFormat>({FileFormat::JAF});
	case FunctionNode:
		return QVector<FileFormat>({FileFormat::JAM});
	case ExStringKeyValueNode:
	case ExIntKeyValueNode:
	case ExRowNode:
		return QVector<FileFormat>({FileFormat::TXTEX});
	case FileNode:
		return getSupportedConversionFormats(
				extensionToFileFormat(
					QFileInfo(ar.file->name).suffix()));
	}
	return QVector<FileFormat>();
}

static bool convertFormat(struct port *port, struct archive_data *dfile, FileFormat from, FileFormat to)
{
	if (from == to) {
		return port_write_bytes(port, dfile->data, dfile->size);
	}

	struct cg *cg;
	enum cg_type cg_type;
	switch (from) {
	case FileFormat::PNG:
	case FileFormat::WEBP:
	case FileFormat::QNT:
	case FileFormat::AJP:
	case FileFormat::DCF:
	case FileFormat::PCF:
		switch (to) {
		case FileFormat::PNG:  cg_type = ALCG_PNG;  break;
		case FileFormat::WEBP: cg_type = ALCG_WEBP; break;
		case FileFormat::QNT:  cg_type = ALCG_QNT;  break;
		default: return false;
		}
		// FIXME: cg_write can only write to files...
		if (port->type != PORT_TYPE_FILE)
			return false;
		if (!(cg = cg_load_data(dfile)))
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
	case FileFormat::ACX:
	case FileFormat::JAF:
	case FileFormat::JAM:
	case FileFormat::AAR:
	case FileFormat::AFA:
	case FileFormat::ALD:
	case FileFormat::ALK:
	case FileFormat::DLF:
	case FileFormat::FLAT:
		return false;;
	}
	return false;
}

bool NavigatorNode::write(struct port *port, FileFormat format) const
{
	bool r;
	switch (type) {
	case RootNode:
	case BranchNode:
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
		_ain_disassemble_function(port, ainItem.ainFile, ainItem.i, DASM_WARN_ON_ERROR);
		return true;
	case EnumNode:
		if (format != FileFormat::JAF)
			return false;
		set_encodings("UTF-8", "UTF-8");
		ain_dump_enum(port, ainItem.ainFile, ainItem.i);
		return true;
	case GlobalNode:
		if (format != FileFormat::JAF)
			return false;
		set_encodings("UTF-8", "UTF-8");
		ain_dump_global(port, ainItem.ainFile, ainItem.i);
		return true;
	case FuncTypeNode:
	case DelegateNode:
		if (format != FileFormat::JAF)
			return false;
		set_encodings("UTF-8", "UTF-8");
		ain_dump_functype(port, ainItem.ainFile, ainItem.i, type == DelegateNode);
		return true;
	case LibraryNode:
		if (format != FileFormat::JAF)
			return false;
		set_encodings("UTF-8", "UTF-8");
		ain_dump_library(port, ainItem.ainFile, ainItem.i);
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
		r = convertFormat(port, ar.file,
				extensionToFileFormat(QFileInfo(ar.file->name).suffix()),
				format);
		archive_release_file(ar.file);
		return r;
	}
	return false;
}

void NavigatorNode::open(bool newTab) const
{
	struct port port;
	char *data;
	switch (type) {
	case RootNode:
	case BranchNode:
		break;
	case ClassNode:
	case EnumNode:
	case GlobalNode:
	case FuncTypeNode:
	case DelegateNode:
	case LibraryNode:
		port_buffer_init(&port);
		write(&port, FileFormat::JAF);
		data = (char*)port_buffer_get(&port, NULL);
		GAlice::openText(getName(), data, FileFormat::JAF, newTab);
		free(data);
		break;
	case FunctionNode:
		GAlice::openAinFunction(ainItem.ainFile, ainItem.i, newTab);
		break;
	case ExStringKeyValueNode:
		GAlice::openExValue(QString::fromUtf8(exKV.key.s->text), exKV.value, newTab);
		break;
	case ExIntKeyValueNode:
		GAlice::openExValue("[" + QString::number(exKV.key.i) + "]", exKV.value, newTab);
		break;
	case ExRowNode:
		// TODO
		break;
	case FileNode:
		switch (ar.type) {
		case NormalFile:
			GAlice::openArchiveData(ar.file, newTab);
			break;
		case ExFile:
		case ArFile:
			break;
		}
		break;
	}
}

static QString exRowName(struct ex_table *t, unsigned row)
{
	int index_col = -1;
	for (unsigned i = 0; i < t->nr_columns; i++) {
		if (t->fields[i].is_index) {
			index_col = i;
			break;
		}
	}

	if (index_col >= 0) {
		struct ex_value *v = &t->rows[row][index_col];
		switch (v->type) {
		case EX_INT:
			return QString::number(v->i);
		case EX_FLOAT:
			return QString::number(v->f);
		case EX_STRING:
			return v->s->text;
		default:
			break;
		}
	}

	return "[" + QString::number(row) + "]";
}

QString NavigatorNode::getName() const
{
	switch (type) {
	case RootNode:
		return "Name";
	case BranchNode:
		return name;
	case ClassNode:
		return QString::fromUtf8(ainItem.ainFile->structures[ainItem.i].name);
	case FunctionNode:
		return QString::fromUtf8(ainItem.ainFile->functions[ainItem.i].name);
	case EnumNode:
		return QString::fromUtf8(ainItem.ainFile->enums[ainItem.i].name);
	case GlobalNode:
		return QString::fromUtf8(ainItem.ainFile->globals[ainItem.i].name);
	case FuncTypeNode:
		return QString::fromUtf8(ainItem.ainFile->function_types[ainItem.i].name);
	case DelegateNode:
		return QString::fromUtf8(ainItem.ainFile->delegates[ainItem.i].name);
	case LibraryNode:
		return QString::fromUtf8(ainItem.ainFile->libraries[ainItem.i].name);
	case ExStringKeyValueNode:
		return QString::fromUtf8(exKV.key.s->text);
	case ExIntKeyValueNode:
		return "[" + QString::number(exKV.key.i) + "]";
	case ExRowNode:
		return exRowName(exRow.t, exRow.i);
	case FileNode:
		return ar.file->name;
	}
	return "?";
}

QVariant NavigatorNode::getType() const
{
	switch (type) {
		case RootNode:
			return "Type";
		case BranchNode:
		case ClassNode:
		case FunctionNode:
		case EnumNode:
		case GlobalNode:
		case FuncTypeNode:
		case DelegateNode:
		case LibraryNode:
		case FileNode:
			break;
		case ExStringKeyValueNode:
		case ExIntKeyValueNode:
			return ex_strtype(exKV.value->type);
		case ExRowNode:
			return "row";
	}
	return QVariant();
}

QVariant NavigatorNode::getValue() const
{
	switch (type) {
	case RootNode:
		return "Value";
	case BranchNode:
	case ClassNode:
	case FunctionNode:
	case EnumNode:
	case GlobalNode:
	case FuncTypeNode:
	case DelegateNode:
	case LibraryNode:
	case ExRowNode:
	case FileNode:
		break;
	case ExStringKeyValueNode:
	case ExIntKeyValueNode:
		switch (exKV.value->type) {
		case EX_INT:    return exKV.value->i;
		case EX_FLOAT:  return exKV.value->f;
		case EX_STRING: return QString::fromUtf8(exKV.value->s->text);
		default:        break;
		}
		break;
	}
	return QVariant();
}
