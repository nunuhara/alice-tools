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

#include "ex_model.hpp"

extern "C" {
#include "system4/ex.h"
#include "system4/string.h"
#include "alice/port.h"
}

ExModel::ExNode::ExNode(struct ex *exFile)
        : type(EX_NODE_ROOT)
        , parentNode(nullptr)
{
        for (unsigned i = 0; i < exFile->nr_blocks; i++) {
                appendChild(new ExNode(exFile->blocks[i].name->text, &exFile->blocks[i].val));
        }
}

ExModel::ExNode::ExNode(const char *key, struct ex_value *value)
        : type(EX_NODE_S_KEYVALUE)
        , parentNode(nullptr)
{
        kv.key.s = key;
        kv.value = value;
        appendChildren(value);
}

ExModel::ExNode::ExNode(unsigned i, struct ex_value *value)
        : type(EX_NODE_I_KEYVALUE)
        , parentNode(nullptr)
{
        kv.key.i = i;
        kv.value = value;
        appendChildren(value);
}

/*
 * Constructor for table row.
 */
ExModel::ExNode::ExNode(unsigned r, struct ex_table *table, struct ex_field *fields, unsigned nr_fields)
        : type(EX_NODE_ROW)
        , parentNode(nullptr)
{
        row.i = r;
        row.t = table;

        if (nr_fields != table->nr_columns) {
                qWarning("Field/Column count mismatch");
                if (table->nr_columns < nr_fields)
                        nr_fields = table->nr_columns;
        }

        // add columns for row
        for (unsigned col = 0; col < nr_fields; col++) {
                appendChild(new ExNode(&table->rows[r][col], &fields[col]));
        }
}

/*
 * Constructor for table column.
 */
ExModel::ExNode::ExNode(struct ex_value *value, struct ex_field *field)
        : type(EX_NODE_S_KEYVALUE)
        , parentNode(nullptr)
{
        kv.key.s = field->name->text;
        kv.value = value;

        if (value->type != EX_TABLE)
                return;

        // add rows for sub-table
        for (unsigned i = 0; i < value->t->nr_rows; i++) {
                appendChild(new ExNode(i, value->t, field->subfields, field->nr_subfields));
        }
}

void ExModel::ExNode::appendChildren(struct ex_value *value)
{
        switch (value->type) {
        case EX_TABLE:
                for (unsigned i = 0; i < value->t->nr_rows; i++) {
                        appendChild(new ExNode(i, value->t, value->t->fields, value->t->nr_fields));
                }
                break;
        case EX_LIST:
                for (unsigned i = 0; i < value->list->nr_items; i++) {
                        appendChild(new ExNode(i, &value->list->items[i].value));
                }
                break;
        case EX_TREE:
                if (value->tree->is_leaf) {
                        kv.key.s = value->tree->leaf.name->text;
                        kv.value = &value->tree->leaf.value;
                        appendChildren(kv.value);
                } else {
                        for (unsigned i = 0; i < value->tree->nr_children; i++) {
                                appendChild(new ExNode(value->tree->children[i].name->text, &value->tree->_children[i]));
                        }
                }
                break;
        default:
                break;
        }
}

ExModel::ExNode::~ExNode()
{
        qDeleteAll(children);
}

ExModel::ExNode *ExModel::ExNode::child(int i)
{
        if (i < 0 || i >= children.size())
                return nullptr;
        return children[i];
}

ExModel::ExNode *ExModel::ExNode::parent()
{
        return parentNode;
}

void ExModel::ExNode::appendChild(ExModel::ExNode *child)
{
        children.append(child);
        child->parentNode = this;
}

int ExModel::ExNode::child()
{
        if (parentNode)
                return parentNode->children.indexOf(const_cast<ExNode*>(this));
        return 0;
}

int ExModel::ExNode::childCount()
{
        return children.size();
}

int ExModel::ExNode::columnCount()
{
        return 3;
}

QString ExModel::ExNode::typeName() const
{
        switch (type) {
        case EX_NODE_ROOT:
                return "";
        case EX_NODE_S_KEYVALUE:
        case EX_NODE_I_KEYVALUE:
                return ex_strtype(kv.value->type);
        case EX_NODE_ROW:
                return "row";
        }
        return "";
}

QString ExModel::ExNode::name() const
{
        switch (type) {
        case EX_NODE_ROOT:
                return "";
        case EX_NODE_S_KEYVALUE:
                return QString::fromUtf8(kv.key.s);
        case EX_NODE_I_KEYVALUE:
                return "[" + QString::number(kv.key.i) + "]";
        case EX_NODE_ROW:
                // TODO: display index column name
                return QString::number(row.i);
        }
        return "";
}

QVariant ExModel::ExNode::value() const
{
        struct ex_value *v = nullptr;
        switch (type) {
        case EX_NODE_ROOT:
        case EX_NODE_ROW:
                return QVariant();
        case EX_NODE_S_KEYVALUE:
        case EX_NODE_I_KEYVALUE:
                v = kv.value;
                break;
        }
        if (v == nullptr)
                return QVariant();

        switch (v->type) {
        case EX_INT: return v->i;
        case EX_FLOAT: return v->f;
        case EX_STRING: return v->s->text;
        default: return QVariant();
        }
}

QVariant ExModel::ExNode::data(int column) const
{
        if (column == 0) {
                if (type == EX_NODE_ROOT)
                        return "Name";
                return name();
        } else if (column == 1) {
                if (type == EX_NODE_ROOT)
                        return "Type";
                return typeName();
        } else if (column == 2) {
                if (type == EX_NODE_ROOT)
                        return "Value";
                return value();
        }
        return QVariant();
}

ExModel::ExModel(struct ex *data, QObject *parent)
        : QAbstractItemModel(parent)
{
        exFile = data;
        root = new ExNode(data);
}

ExModel::~ExModel()
{
        delete root;
}

QModelIndex ExModel::index(int row, int column, const QModelIndex &parent) const
{
        if (!hasIndex(row, column, parent))
                return QModelIndex();

        ExNode *parentNode;
        if (!parent.isValid())
                parentNode = root;
        else
                parentNode = static_cast<ExNode*>(parent.internalPointer());

        ExNode *childNode = parentNode->child(row);
        if (childNode)
                return createIndex(row, column, childNode);
        return QModelIndex();
}

QModelIndex ExModel::parent(const QModelIndex &index) const
{
        if (!index.isValid())
                return QModelIndex();

        ExNode *childNode = static_cast<ExNode*>(index.internalPointer());
        ExNode *parentNode = childNode->parent();

        if (parentNode == root)
                return QModelIndex();

        return createIndex(parentNode->child(), 0, parentNode);
}

int ExModel::rowCount(const QModelIndex &parent) const
{
        if (parent.column() > 0)
                return 0;

        ExNode *parentNode;
        if (!parent.isValid())
                parentNode = root;
        else
                parentNode = static_cast<ExNode*>(parent.internalPointer());

        return parentNode->childCount();
}

int ExModel::columnCount(const QModelIndex &parent) const
{
        if (parent.isValid())
                return static_cast<ExNode*>(parent.internalPointer())->columnCount();
        return root->columnCount();
}

QVariant ExModel::data(const QModelIndex &index, int role) const
{
        if (!index.isValid())
                return QVariant();

        if (role != Qt::DisplayRole)
                return QVariant();

        ExNode *node = static_cast<ExNode*>(index.internalPointer());
        return node->data(index.column());
}

Qt::ItemFlags ExModel::flags(const QModelIndex &index) const
{
        if (!index.isValid())
                return Qt::NoItemFlags;
        return QAbstractItemModel::flags(index);
}

QVariant ExModel::headerData(int section, Qt::Orientation orientation, int role) const
{
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
                return root->data(section);
        return QVariant();
}

struct ex_value *ExModel::exValue(const QModelIndex &index)
{
        if (!index.isValid())
                return nullptr;

        ExNode *node = static_cast<ExNode*>(index.internalPointer());
        switch (node->type) {
        case EX_NODE_ROOT:
                break;
        case EX_NODE_S_KEYVALUE:
        case EX_NODE_I_KEYVALUE:
                return node->kv.value;
        case EX_NODE_ROW:
                break;
        }
        return nullptr;
}

QString ExModel::exName(const QModelIndex &index)
{
        if (!index.isValid())
                return "";

        ExNode *node = static_cast<ExNode*>(index.internalPointer());
        switch (node->type) {
        case EX_NODE_ROOT:
                break;
        case EX_NODE_S_KEYVALUE:
                return QString::fromUtf8(node->kv.key.s);
                break;
        case EX_NODE_I_KEYVALUE:
                return "[" + QString::number(node->kv.key.i) + "]";
                break;
        case EX_NODE_ROW:
                return "[" + QString::number(node->row.i) + "]";
                break;
        }
        return "";
}
