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

#ifndef GALICE_NAVIGATOR_NODE_HPP
#define GALICE_NAVIGATOR_NODE_HPP

#include <QString>
#include <QVariant>
#include <QVector>
#include "galice.hpp"

class NavigatorNode {
public:
	enum NodeType {
		RootNode,
		BranchNode,
		ClassNode,
		FunctionNode,
		EnumNode,
		GlobalNode,
		FuncTypeNode,
		DelegateNode,
		LibraryNode,
		ExStringKeyValueNode,
		ExIntKeyValueNode,
		ExRowNode,
		FileNode,
	};
	enum NodeFileType {
		NormalFile,
		ExFile,
		ArFile,
	};

	NodeType type;
	union {
		// BranchNode
		const char *name;
		// ClassNode
		// FunctionNode
		struct {
			struct ain *ainFile;
			int i;
		} ainItem;
		// ExStringKeyValueNode
		// ExIntKeyValueNode
		struct {
			struct {
				struct string *s;
				unsigned i;
			} key;
			struct ex_value *value;
		} exKV;
		// ExRowNode
		struct {
			unsigned i;
			struct ex_table *t;
		} exRow;
		// FileNode
		struct {
			// Descriptor for external use
			struct archive_data *file;
			// Persistent loaded descriptor (if needed)
			struct archive_data *data;
			NodeFileType type;
			union {
				struct ex *ex;
				struct archive *ar;
			};
		} ar;
	};

	/*
	 * Get the list of supported formats for writing.
	 */
	QVector<FileFormat> getSupportedFormats() const;

	/*
	 * Write the contents of the node to a port in the given output format.
	 */
	bool write(struct port *port, FileFormat format) const;

	/*
	 * Open a node.
	 */
	void open(bool newTab) const;

	/*
	 * Get the display name of the node.
	 */
	QString getName() const;

	/*
	 * Get the type name of the node.
	 */
	QVariant getType() const;

	/*
	 * Get the value of the node.
	 */
	QVariant getValue() const;
};

#endif /* GALICE_NAVIGATOR_NODE_HPP */
