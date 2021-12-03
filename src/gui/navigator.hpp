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

#ifndef GALICE_NAVIGATOR_HPP
#define GALICE_NAVIGATOR_HPP

#include <QDockWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QVector>

struct ain;
struct ex;

extern "C" {
#include "system4/ain.h"
#include "system4/ex.h"
}

class Navigator : public QDockWidget
{
        Q_OBJECT

public:
        Navigator(QWidget *parent = nullptr);
        ~Navigator();

private slots:
        void addAinFile(const QString &fileName, struct ain *ain);
        void addExFile(const QString &fileName, struct ex *ex);
        //void addArchive(const QString &fileName, struct archive *ar);
        void filesystemOpen(const QModelIndex &index);

signals:
        void fileOpen(const QString &path);
        void openClass(struct ain *ainObj, int i);
        void openFunction(struct ain *ainObj, int i);
        void openExValue(const QString &name, struct ex_value *val);

private:
        void addFilesystem();
        void addFile(const QString &name, QWidget *widget);

        QComboBox *fileSelector;
        QStackedWidget *stack;

        // XXX: Models aren't automatically deleted with the view, so we need
        //      to keep track of them somehow. This solution is simple but will
        //      make closing files more difficult.
        QVector<QAbstractItemModel*> models;
};

#endif /* GALICE_NAVIGATOR_HPP */
