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

#include "ex_table_model.hpp"

extern "C" {
#include "system4/ex.h"
#include "system4/string.h"
#include "alice.h"
}

ExTableModel::ExTableModel(struct ex_table *table, QObject *parent)
	: QAbstractTableModel(parent)
	, table(table)
{
}

int ExTableModel::rowCount(const QModelIndex & /*parent*/) const
{
	return table->nr_rows;
}

int ExTableModel::columnCount(const QModelIndex & /*parent*/) const
{
	return table->nr_columns;
}

QVariant ExTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < (int)table->nr_fields)
		return QString(table->fields[section].name->text);
	if (role == Qt::DisplayRole && orientation == Qt::Vertical)
		return QString::number(section);
	return QVariant();
}

static QString exValueToString(struct ex_value *v)
{
	switch (v->type) {
	case EX_INT:
		return QString::number(v->i);
	case EX_FLOAT:
		return QString::number(v->f);
	case EX_STRING:
		return QString(v->s->text);
	case EX_TABLE:
		return QString("<table ...>");
	case EX_LIST:
		return QString("<list ...>");
	case EX_TREE:
		return QString("<tree ...>");
	}
	return QString("<?>");
}

QVariant ExTableModel::data(const QModelIndex &index, int role) const
{
	if (index.row() >= (int)table->nr_rows || index.column() >= (int)table->nr_columns)
		return QVariant();
	if (role == Qt::DisplayRole)
		return exValueToString(&table->rows[index.row()][index.column()]);
	return QVariant();
}
