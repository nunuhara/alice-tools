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

#ifndef GALICE_AIN_OBJECTS_MODEL_HPP
#define GALICE_AIN_OBJECTS_MODEL_HPP

#include <QAbstractItemModel>
#include <QVector>

struct ain;

class AinObjectsModel : public QAbstractItemModel
{
        Q_OBJECT

public:
        explicit AinObjectsModel(struct ain *data, QObject *parent = nullptr);
        ~AinObjectsModel();

        QVariant data(const QModelIndex &index, int role) const override;
        Qt::ItemFlags flags(const QModelIndex &index) const override;
        QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const override;
        QModelIndex index(int row, int column,
                          const QModelIndex &parent = QModelIndex()) const override;
        QModelIndex parent(const QModelIndex &index) const override;
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;

public slots:
        void open(const QModelIndex &index);

signals:
        void openClass(struct ain *ainObj, int i);
        void openFunction(struct ain *ainObj, int i);

private:
        enum ObjectNodeType {
                OBJECT_NODE_ROOT,
                OBJECT_NODE_CLASS,
                OBJECT_NODE_METHOD,
        };
        class ObjectNode {
        public:
                explicit ObjectNode(enum ObjectNodeType type, int index);
                ~ObjectNode();
                ObjectNode *child(int i);
                ObjectNode *parent();
                void appendChild(ObjectNode *child);
                int row();
                int childCount();
                enum ObjectNodeType type;
                int index;
        private:
                QVector<ObjectNode*> children;
                ObjectNode *parentNode;
        };
        struct ain *ainObject;
        ObjectNode *root;
};

#endif
