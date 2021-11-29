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

#ifndef GALICE_AIN_FUNCTIONS_MODEL_HPP
#define GALICE_AIN_FUNCTIONS_MODEL_HPP

#include <QAbstractListModel>

extern "C" {
#include "system4/ain.h"
}

class AinFunctionsModel : public QAbstractListModel
{
        Q_OBJECT

public:
        AinFunctionsModel(struct ain *ain, QObject *parent = nullptr)
                : QAbstractListModel(parent), ain(ain) {}
        ~AinFunctionsModel();// { ain_free(ain); };

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const override;

private:
        struct ain *ain;
};

#endif /* GALICE_AIN_FUNCTIONS_MODEL_HPP */
