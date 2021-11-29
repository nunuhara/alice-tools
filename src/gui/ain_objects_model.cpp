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

#include <cstring>
#include "ain_objects_model.hpp"

extern "C" {
#include "system4/ain.h"
}

AinObjectsModel::ObjectNode::ObjectNode(enum ObjectNodeType type, int index)
        : type(type)
        , index(index)
        , parentNode(nullptr)
{
}

AinObjectsModel::ObjectNode::~ObjectNode()
{
        qDeleteAll(children);
}

AinObjectsModel::ObjectNode *AinObjectsModel::ObjectNode::child(int i)
{
        if (i < 0 || i >= children.size())
                return nullptr;
        return children[i];
}

AinObjectsModel::ObjectNode *AinObjectsModel::ObjectNode::parent()
{
        return parentNode;
}

void AinObjectsModel::ObjectNode::appendChild(AinObjectsModel::ObjectNode *child)
{
        children.append(child);
        child->parentNode = this;
}

int AinObjectsModel::ObjectNode::row()
{
        if (parentNode)
                return parentNode->children.indexOf(const_cast<ObjectNode*>(this));
        return 0;
}

int AinObjectsModel::ObjectNode::childCount()
{
        return children.size();
}

AinObjectsModel::AinObjectsModel(struct ain *data, QObject *parent)
        : QAbstractItemModel(parent)
{
        ain = data;

        root = new ObjectNode(OBJECT_NODE_ROOT, 0);

        for (int i = 0; i < ain->nr_structures; i++) {
                root->appendChild(new ObjectNode(OBJECT_NODE_CLASS, i));
        }

        for (int i = 0; i < ain->nr_functions; i++) {
                int struct_type = ain->functions[i].struct_type;
                if (struct_type >= 0) {
                        root->child(struct_type)->appendChild(new ObjectNode(OBJECT_NODE_METHOD, i));
                }
        }
}

AinObjectsModel::~AinObjectsModel()
{
        delete root;
}

QModelIndex AinObjectsModel::index(int row, int column, const QModelIndex &parent) const
{
        if (!hasIndex(row, column, parent))
                return QModelIndex();

        ObjectNode *parentNode;
        if (!parent.isValid())
                parentNode = root;
        else
                parentNode = static_cast<ObjectNode*>(parent.internalPointer());

        ObjectNode *childNode = parentNode->child(row);
        if (childNode)
                return createIndex(row, column, childNode);
        return QModelIndex();
}

QModelIndex AinObjectsModel::parent(const QModelIndex &index) const
{
        if (!index.isValid())
                return QModelIndex();

        ObjectNode *childNode = static_cast<ObjectNode*>(index.internalPointer());
        ObjectNode *parentNode = childNode->parent();

        if (parentNode == root)
                return QModelIndex();

        return createIndex(parentNode->row(), 0, parentNode);
}

int AinObjectsModel::rowCount(const QModelIndex &parent) const
{
        if (parent.column() > 0)
                return 0;

        ObjectNode *parentNode;
        if (!parent.isValid()) {
                parentNode = root;
        } else {
                parentNode = static_cast<ObjectNode*>(parent.internalPointer());
        }

        return parentNode->childCount();
}

int AinObjectsModel::columnCount(const QModelIndex &parent) const
{
        return 1;
}

QVariant AinObjectsModel::data(const QModelIndex &index, int role) const
{
        if (!index.isValid())
                return QVariant();

        if (role != Qt::DisplayRole)
                return QVariant();

        ObjectNode *node = static_cast<ObjectNode*>(index.internalPointer());
        switch (node->type) {
        case OBJECT_NODE_ROOT:
                return QVariant();
        case OBJECT_NODE_CLASS:
                return ain->structures[node->index].name;
        case OBJECT_NODE_METHOD: {
                char *name = strchr(ain->functions[node->index].name, '@');
                if (name)
                        return name+1;
                return ain->functions[node->index].name;
        }
        }

        return QVariant();
}

Qt::ItemFlags AinObjectsModel::flags(const QModelIndex &index) const
{
        if (!index.isValid())
                return Qt::NoItemFlags;
        return QAbstractItemModel::flags(index);
}

QVariant AinObjectsModel::headerData(int section, Qt::Orientation, int role) const
{
        return QVariant();
}
