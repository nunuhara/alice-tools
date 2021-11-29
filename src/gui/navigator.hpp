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

struct ain;

class Navigator : public QDockWidget
{
        Q_OBJECT

public:
        Navigator(QWidget *parent = nullptr);

private slots:
        void addAinFile(const QString &fileName, struct ain *ain);
        //void addExFile(const QString &fileName, struct ex *ex);
        //void addArchiveFile(const QString &fileName, struct archive *ar);
        void filesystemOpen(const QModelIndex &index);

signals:
        void fileOpen(const QString &path);

private:
        void addFilesystem();
        void addFile(const QString &name, QWidget *widget);

        QComboBox *fileSelector;
        QStackedWidget *stack;
};

#endif /* GALICE_NAVIGATOR_HPP */
