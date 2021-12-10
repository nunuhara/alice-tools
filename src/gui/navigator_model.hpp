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

#include <QAbstractItemModel>
#include <QVector>

class NavigatorModel : public QAbstractItemModel {
        Q_OBJECT
public:
        static NavigatorModel *fromExFile(struct ex *exFile, QObject *parent = nullptr);
        static NavigatorModel *fromAinClasses(struct ain *ainFile, QObject *parent = nullptr);
        static NavigatorModel *fromAinFunctions(struct ain *ainFile, QObject *parent = nullptr);
        static NavigatorModel *fromArchive(struct archive *ar, QObject *parent = nullptr);
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

public slots:
        void requestOpen(const QModelIndex &index) const;
        void requestOpenNewTab(const QModelIndex &index) const;

signals:
        void requestedOpenClass(struct ain *ainFile, int i, bool newTab) const;
        void requestedOpenFunction(struct ain *ainFile, int i, bool newTab) const;
        void requestedOpenExValue(const QString &name, struct ex_value *value, bool newTab) const;
        void requestedOpenArchiveFile(struct archive_data *data, bool newTab) const;

private:
        explicit NavigatorModel(QObject *parent = nullptr)
                : QAbstractItemModel(parent) {}

        enum NodeType {
                RootNode,
                ClassNode,
                FunctionNode,
                ExStringKeyValueNode,
                ExIntKeyValueNode,
                ExRowNode,
                FileNode,
        };
        class Node {
        public:
                static Node *fromEx(struct ex *exFile);
                static Node *fromAinClasses(struct ain *ainFile);
                static Node *fromAinFunctions(struct ain *ainFile);
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

        private:
                Node(NodeType type) : type(type), parentNode(nullptr) {}
                static Node *fromExKeyValue(const char *key, struct ex_value *value);
                static Node *fromExKeyValue(int key, struct ex_value *value);
                static Node *fromExRow(int index, struct ex_table *table, struct ex_field *fields, unsigned nFields);
                static Node *fromExColumn(struct ex_value *value, struct ex_field *field);
                void appendExValueChildren(struct ex_value *value);
                static Node *fromAinClass(struct ain *ainFile, int index);
                static Node *fromAinFunction(struct ain *ainFile, int index);
                static Node *fromArchiveFile(struct archive_data *file);
                static void fromArchiveIter(struct archive_data *data, void *user);

                NodeType type;
                union {
                        // ClassNode
                        // FunctionNode
                        struct {
                                struct ain *ainFile;
                                int i;
                        } ainItem;
                        // ExStringKeyValueNode
                        // ExIntKeyValueNode
                        struct {
                                struct {
                                        const char *s;
                                        unsigned i;
                                } key;
                                struct ex_value *value;
                        } exKV;
                        // ExRowNode
                        struct {
                                unsigned i;
                                struct ex_table *t;
                        } exRow;
                        // FileNode
                        struct archive_data *arFile;
                };
                QVector<Node*> children;
                Node *parentNode;
        };
        Node *root;
};

#endif /* GALICE_NAVIGATOR_MODEL_HPP */
