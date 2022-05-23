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

#include <memory>
#include <QDockWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QVector>

struct ain;
struct ex;
class MainWindow;

extern "C" {
#include "system4/ain.h"
#include "system4/ex.h"
#include "system4/archive.h"
}

class Navigator : public QDockWidget
{
        Q_OBJECT

public:
        Navigator(MainWindow *parent = nullptr);
        ~Navigator();

private slots:
        void addAinFile(const QString &fileName, std::shared_ptr<struct ain> ain);
        void addExFile(const QString &fileName, std::shared_ptr<struct ex> ex);
        void addArchive(const QString &fileName, std::shared_ptr<struct archive> ar);

private:
        void addFilesystem();
        void addFile(const QString &name, QWidget *widget);

        MainWindow *window;
        QComboBox *fileSelector;
        QStackedWidget *stack;
};

#endif /* GALICE_NAVIGATOR_HPP */
