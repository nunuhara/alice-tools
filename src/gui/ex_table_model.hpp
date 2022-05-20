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

#ifndef GALICE_EX_TABLE_MODEL_HPP
#define GALICE_EX_TABLE_MODEL_HPP

#include <QAbstractTableModel>
#include <QVector>

struct ex_table;

class ExTableModel : public QAbstractTableModel {
	Q_OBJECT
public:
	ExTableModel(struct ex_table *table, QObject *parent = nullptr);
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant headerData(int section, Qt::Orientation orientation,
			int role = Qt::DisplayRole) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
private:
	struct ex_table *table;
};

#endif /* GALICE_EX_TABLE_MODEL_HPP */
