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

#ifndef GALICE_EX_MODEL_HPP
#define GALICE_EX_MODEL_HPP

#include <QAbstractItemModel>
#include <QVector>

struct ex;
struct ex_value;
struct ex_block;
struct ex_table;
struct ex_list;
struct ex_tree;

class ExModel : public QAbstractItemModel
{
        Q_OBJECT

public:
        explicit ExModel(struct ex *data, QObject *parent = nullptr);
        ~ExModel();

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
        void openExValue(const QString &name, struct ex_value *value);

private:
        enum ExNodeType {
                EX_NODE_ROOT,
                EX_NODE_S_KEYVALUE,
                EX_NODE_I_KEYVALUE,
                EX_NODE_ROW,
        };
        class ExNode {
        public:
                explicit ExNode(struct ex *exFile);
                explicit ExNode(const char *key, struct ex_value *value);
                explicit ExNode(unsigned i, struct ex_value *value);
                explicit ExNode(unsigned r, struct ex_table *table, struct ex_field *field, unsigned nr_fields);
                explicit ExNode(struct ex_value *value, struct ex_field *field);
                ~ExNode();
                void appendChild(ExNode *child);
                ExNode *child(int i);
                int childCount();
                int columnCount();
                QVariant data(int column) const;
                int child();
                ExNode *parent();
                enum ExNodeType type;
                union {
                        struct {
                                struct {
                                        const char *s;
                                        unsigned i;
                                } key;
                                struct ex_value *value;
                        } kv;
                        struct {
                                unsigned i;
                                struct ex_table *t;
                        } row;
                };
        private:
                void appendChildren(struct ex_value *value);
                QString name() const;
                QString typeName() const;
                QVariant value() const;
                QVector<ExNode*> children;
                ExNode *parentNode;
        };
        ExNode *readBlock(struct ex_block *block);
        ExNode *readTree(struct ex_tree *value);
        void readList(ExNode *parent, struct ex_list *list);
        void readTable(ExNode *parent, struct ex_table *table);
        ExNode *root;
        struct ex *exFile;
};

#endif /* GALICE_EX_MODEL_HPP */
