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

#ifndef GALICE_NAVIGATOR_MODEL_HPP
#define GALICE_NAVIGATOR_MODEL_HPP

#include <memory>
#include <QAbstractItemModel>
#include <QVector>
#include "navigator_node.hpp"

class NavigatorModel : public QAbstractItemModel {
        Q_OBJECT
public:
        static NavigatorModel *fromExFile(std::shared_ptr<struct ex> exFile, QObject *parent = nullptr);
	static NavigatorModel *fromAinFile(std::shared_ptr<struct ain> ainFile, QObject *parent = nullptr);
        static NavigatorModel *fromArchive(std::shared_ptr<struct archive> ar, QObject *parent = nullptr);
        ~NavigatorModel();

        QVariant data(const QModelIndex &index, int role) const override;
        Qt::ItemFlags flags(const QModelIndex &index) const override;
        QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const override;
        QModelIndex index(int row, int column,
                          const QModelIndex &parent = QModelIndex()) const override;
        QModelIndex parent(const QModelIndex &index) const override;
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	NavigatorNode *getNode(const QModelIndex &index) const;

private:
        explicit NavigatorModel(QObject *parent = nullptr)
                : QAbstractItemModel(parent) {}

        class Node {
        public:
                static Node *fromEx(struct ex *exFile);
                static Node *fromAin(struct ain *ainFile);
                static Node *fromArchive(struct archive *ar);
                ~Node();

                void appendChild(Node *child);
                Node *child(int i);
                int childCount();
                int columnCount();
                QVariant data(int column) const;
                int row();
                Node *parent();
                void requestOpen(const NavigatorModel *model, bool newTab);

		NavigatorNode node;

        private:
		Node(NavigatorNode::NodeType type);
                static Node *fromExKeyValue(struct string *key, struct ex_value *value);
                static Node *fromExKeyValue(int key, struct ex_value *value);
                static Node *fromExRow(int index, struct ex_table *table, struct ex_field *fields, unsigned nFields);
                static Node *fromExColumn(struct ex_value *value, struct ex_field *field);
                void appendExValueChildren(struct ex_value *value);
                void appendExFileChildren(struct ex *exFile);
                void appendArchiveChildren(struct archive *archive);
		static Node *fromAinItem(struct ain *ain, int i, NavigatorNode::NodeType type);
                static Node *fromArchiveFile(struct archive_data *file);
                static void fromArchiveIter(struct archive_data *data, void *user);

                QVector<Node*> children;
                Node *parentNode;
        };
        Node *root;
	std::shared_ptr<struct ex> exFile;
	std::shared_ptr<struct ain> ainFile;
	std::shared_ptr<struct archive> arFile;
};

#endif /* GALICE_NAVIGATOR_MODEL_HPP */
