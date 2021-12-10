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

#include "navigator_model.hpp"

extern "C" {
#include "system4/ain.h"
#include "system4/archive.h"
#include "system4/ex.h"
#include "system4/string.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/port.h"
}

NavigatorModel::Node *NavigatorModel::Node::fromEx(struct ex *ex)
{
        Node *root = new Node(RootNode);

        for (unsigned i = 0; i < ex->nr_blocks; i++) {
                root->appendChild(Node::fromExKeyValue(ex->blocks[i].name->text, &ex->blocks[i].val));
        }

        return root;
}

NavigatorModel::Node *NavigatorModel::Node::fromExKeyValue(const char *key, struct ex_value *value)
{
        Node *node = new Node(ExStringKeyValueNode);
        node->exKV.key.s = key;
        node->exKV.value = value;
        node->appendExValueChildren(value);
        return node;
}

NavigatorModel::Node *NavigatorModel::Node::fromExKeyValue(int key, struct ex_value *value)
{
        Node *node = new Node(ExIntKeyValueNode);
        node->exKV.key.i = key;
        node->exKV.value = value;
        node->appendExValueChildren(value);
        return node;
}

NavigatorModel::Node *NavigatorModel::Node::fromExRow(int index, struct ex_table *table, struct ex_field *fields, unsigned nFields)
{
        Node *node = new Node(ExRowNode);
        node->exRow.i = index;
        node->exRow.t = table;

        if (nFields != table->nr_fields) {
                qWarning("Field/Column count mismatch");
                if (table->nr_columns < nFields)
                        nFields = table->nr_columns;
        }

        // add columns for row
        for (unsigned col = 0; col < nFields; col++) {
                node->appendChild(Node::fromExColumn(&table->rows[index][col], &fields[col]));
        }

        return node;
}

NavigatorModel::Node *NavigatorModel::Node::fromExColumn(struct ex_value *value, struct ex_field *field)
{
        Node *node = new Node(ExStringKeyValueNode);
        node->exKV.key.s = field->name->text;
        node->exKV.value = value;

        if (value->type != EX_TABLE)
                return node;

        // add rows for sub-table
        for (unsigned i = 0; i < value->t->nr_rows; i++) {
                node->appendChild(Node::fromExRow(i, value->t, field->subfields, field->nr_subfields));
        }

        return node;
}

void NavigatorModel::Node::appendExValueChildren(struct ex_value *value)
{
        switch (value->type) {
        case EX_TABLE:
                for (unsigned i = 0; i < value->t->nr_rows; i++) {
                        appendChild(Node::fromExRow(i, value->t, value->t->fields, value->t->nr_fields));
                }
                break;
        case EX_LIST:
                for (unsigned i = 0; i < value->list->nr_items; i++) {
                        appendChild(Node::fromExKeyValue(i, &value->list->items[i].value));
                }
                break;
        case EX_TREE:
                if (value->tree->is_leaf) {
                        exKV.key.s = value->tree->leaf.name->text;
                        exKV.value = &value->tree->leaf.value;
                        appendExValueChildren(exKV.value);
                } else {
                        for (unsigned i = 0; i < value->tree->nr_children; i++) {
                                appendChild(Node::fromExKeyValue(value->tree->children[i].name->text, &value->tree->_children[i]));
                        }
                }
                break;
        default:
                break;
        }
}

NavigatorModel::Node *NavigatorModel::Node::fromAinClasses(struct ain *ain)
{
        Node *root = new Node(RootNode);

        // add classes
        for (int i = 0; i < ain->nr_structures; i++) {
                root->appendChild(Node::fromAinClass(ain, i));
        }
        // add methods
        for (int i = 0; i < ain->nr_functions; i++) {
                Node *node = root->child(ain->functions[i].struct_type);
                if (node)
                        node->appendChild(Node::fromAinFunction(ain, i));
        }

        return root;
}

NavigatorModel::Node *NavigatorModel::Node::fromAinFunctions(struct ain *ain)
{
        Node *root = new Node(RootNode);

        for (int i = 0; i < ain->nr_functions; i++) {
                root->appendChild(Node::fromAinFunction(ain, i));
        }

        return root;
}

NavigatorModel::Node *NavigatorModel::Node::fromAinClass(struct ain *ain, int i)
{
        Node *node = new Node(ClassNode);
        node->ainItem.ainFile = ain;
        node->ainItem.i = i;
        return node;
}

NavigatorModel::Node *NavigatorModel::Node::fromAinFunction(struct ain *ain, int i)
{
        Node *node = new Node(FunctionNode);
        node->ainItem.ainFile = ain;
        node->ainItem.i = i;
        return node;
}

void NavigatorModel::Node::fromArchiveIter(struct archive_data *data, void *user)
{
        Node *parent = static_cast<Node*>(user);
        Node *child = new Node(FileNode);
        child->arFile = archive_copy_descriptor(data);
        parent->appendChild(child);
}

NavigatorModel::Node *NavigatorModel::Node::fromArchive(struct archive *ar)
{
        Node *root = new Node(RootNode);
        archive_for_each(ar, fromArchiveIter, root);
        return root;
}

NavigatorModel::Node::~Node()
{
        qDeleteAll(children);

        if (type == FileNode) {
                archive_free_data(arFile);
        }
}

NavigatorModel::Node *NavigatorModel::Node::child(int i)
{
        if (i < 0 || i >= children.size())
                return nullptr;
        return children[i];
}

NavigatorModel::Node *NavigatorModel::Node::parent()
{
        return parentNode;
}

void NavigatorModel::Node::appendChild(NavigatorModel::Node *child)
{
        children.append(child);
        child->parentNode = this;
}

int NavigatorModel::Node::row()
{
        if (parentNode)
                return parentNode->children.indexOf(const_cast<Node*>(this));
        return 0;
}

int NavigatorModel::Node::childCount()
{
        return children.size();
}

int NavigatorModel::Node::columnCount()
{
        return 3;
}

QVariant NavigatorModel::Node::data(int column) const
{
        if (column == 0) {
                switch (type) {
                case RootNode:
                        return "Name";
                case ClassNode:
                        return QString::fromUtf8(ainItem.ainFile->structures[ainItem.i].name);
                case FunctionNode:
                        return QString::fromUtf8(ainItem.ainFile->functions[ainItem.i].name);
                case ExStringKeyValueNode:
                        return QString::fromUtf8(exKV.key.s);
                case ExIntKeyValueNode:
                        return "[" + QString::number(exKV.key.i) + "]";
                case ExRowNode:
                        return "[" + QString::number(exRow.i) + "]";
                case FileNode:
                        return arFile->name;
                }
        } else if (column == 1) {
                switch (type) {
                case RootNode:
                        return "Type";
                case ClassNode:
                case FunctionNode:
                case FileNode:
                        break;
                case ExStringKeyValueNode:
                case ExIntKeyValueNode:
                        return ex_strtype(exKV.value->type);
                case ExRowNode:
                        return "row";
                }
        } else if (column == 2) {
                switch (type) {
                case RootNode:
                        return "Value";
                case ClassNode:
                case FunctionNode:
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
        }
        return QVariant();
}

void NavigatorModel::Node::requestOpen(const NavigatorModel *model, bool newTab)
{
        switch (type) {
        case RootNode:
                break;
        case ClassNode:
                emit model->requestedOpenClass(ainItem.ainFile, ainItem.i, newTab);
                break;
        case FunctionNode:
                emit model->requestedOpenFunction(ainItem.ainFile, ainItem.i, newTab);
                break;
        case ExStringKeyValueNode:
                emit model->requestedOpenExValue(QString::fromUtf8(exKV.key.s), exKV.value, newTab);
                break;
        case ExIntKeyValueNode:
                emit model->requestedOpenExValue("[" + QString::number(exKV.key.i) + "]", exKV.value, newTab);
                break;
        case ExRowNode:
                // TODO
                break;
        case FileNode:
                emit model->requestedOpenArchiveFile(arFile, newTab);
                break;
        }
}

NavigatorModel *NavigatorModel::fromExFile(struct ex *ex, QObject *parent)
{
        NavigatorModel *model = new NavigatorModel(parent);
        model->root = Node::fromEx(ex);
        return model;
}

NavigatorModel *NavigatorModel::fromAinClasses(struct ain *ain, QObject *parent)
{
        NavigatorModel *model = new NavigatorModel(parent);
        model->root = Node::fromAinClasses(ain);
        return model;
}

NavigatorModel *NavigatorModel::fromAinFunctions(struct ain *ain, QObject *parent)
{
        NavigatorModel *model = new NavigatorModel(parent);
        model->root = Node::fromAinFunctions(ain);
        return model;
}

NavigatorModel *NavigatorModel::fromArchive(struct archive *ar, QObject *parent)
{
        NavigatorModel *model = new NavigatorModel(parent);
        model->root = Node::fromArchive(ar);
        return model;
}

NavigatorModel::~NavigatorModel()
{
        delete root;
}

QModelIndex NavigatorModel::index(int row, int column, const QModelIndex &parent) const
{
        if (!hasIndex(row, column, parent))
                return QModelIndex();

        Node *parentNode;
        if (!parent.isValid())
                parentNode = root;
        else
                parentNode = static_cast<Node*>(parent.internalPointer());

        Node *childNode = parentNode->child(row);
        if (childNode)
                return createIndex(row, column, childNode);
        return QModelIndex();
}

QModelIndex NavigatorModel::parent(const QModelIndex &index) const
{
        if (!index.isValid())
                return QModelIndex();

        Node *childNode = static_cast<Node*>(index.internalPointer());
        Node *parentNode = childNode->parent();

        if (!parentNode || parentNode == root)
                return QModelIndex();

        return createIndex(parentNode->row(), 0, parentNode);
}

int NavigatorModel::rowCount(const QModelIndex &parent) const
{
        if (parent.column() > 0)
                return 0;

        Node *parentNode;
        if (!parent.isValid())
                parentNode = root;
        else
                parentNode = static_cast<Node*>(parent.internalPointer());

        return parentNode->childCount();
}

int NavigatorModel::columnCount(const QModelIndex &parent) const
{
        if (parent.isValid())
                return static_cast<Node*>(parent.internalPointer())->columnCount();
        return root->columnCount();
}

QVariant NavigatorModel::data(const QModelIndex &index, int role) const
{
        if (!index.isValid())
                return QVariant();

        if (role != Qt::DisplayRole)
                return QVariant();

        Node *node = static_cast<Node*>(index.internalPointer());
        return node->data(index.column());
}

Qt::ItemFlags NavigatorModel::flags(const QModelIndex &index) const
{
        if (!index.isValid())
                return Qt::NoItemFlags;
        return QAbstractItemModel::flags(index);
}

QVariant NavigatorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
                return root->data(section);
        return QVariant();
}

void NavigatorModel::requestOpen(const QModelIndex &index) const
{
        if (!index.isValid())
                return;
        static_cast<Node*>(index.internalPointer())->requestOpen(this, false);
}

void NavigatorModel::requestOpenNewTab(const QModelIndex &index) const
{
        if (!index.isValid())
                return;
        static_cast<Node*>(index.internalPointer())->requestOpen(this, true);
}
