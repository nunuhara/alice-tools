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

#include "acx_model.hpp"

extern "C" {
#include "system4/acx.h"
#include "system4/string.h"
#include "alice.h"
}

AcxModel::AcxModel(std::shared_ptr<struct acx> acx, QObject *parent)
	: QAbstractTableModel(parent)
	, acx(acx)
{
}

int AcxModel::rowCount(const QModelIndex & /*parent*/) const
{
	return acx->nr_lines;
}

int AcxModel::columnCount(const QModelIndex & /*parent*/) const
{
	return acx->nr_columns;
}

QVariant AcxModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
		return QString::number(section);
	return QVariant();
}

static QString acxValueToString(struct acx *acx, int row, int col)
{
	if (acx->column_types[col] == ACX_STRING)
		return QString(acx_get_string(acx, row, col)->text);
	return QString::number(acx_get_int(acx, row, col));
}

QVariant AcxModel::data(const QModelIndex &index, int role) const
{
	if (index.row() >= acx->nr_lines || index.column() >= acx->nr_columns)
		return QVariant();
	if (role == Qt::DisplayRole)
		return acxValueToString(acx.get(), index.row(), index.column());
	return QVariant();
}
