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

#include <iostream>
#include "ain_functions_model.hpp"

AinFunctionsModel::~AinFunctionsModel()
{
}

int AinFunctionsModel::rowCount(const QModelIndex &parent) const
{
        return ain->nr_functions;
}

QVariant AinFunctionsModel::data(const QModelIndex &index, int role) const
{
        if (!index.isValid())
                return QVariant();

        if (index.row() >= ain->nr_functions)
                return QVariant();

        if (role == Qt::DisplayRole) {
                return ain->functions[index.row()].name;
        }
        return QVariant();
}

QVariant AinFunctionsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
        if (role != Qt::DisplayRole)
                return QVariant();

        if (orientation == Qt::Horizontal)
                return QStringLiteral("Column %1").arg(section);
        else
                return QStringLiteral("Row %1").arg(section);
}
