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

#ifndef GALICE_MAINWINDOW_HPP
#define GALICE_MAINWINDOW_HPP

#include <QMainWindow>
#include <QFileInfo>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTabWidget>
#include "navigator.hpp"

struct ain;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

protected:
        void closeEvent(QCloseEvent *event) override;

private slots:
        void open();
        void about();
        void openError(const QString &filename, const QString &message);
        void openClass(struct ain *ain, int i);
        void openFunction(struct ain *ain, int i);
        void closeTab(int index);

private:
        void createActions();
        void createStatusBar();
        void createDockWindows();
        void readSettings();
        void writeSettings();
        void setupViewer();

        void openText(const QString &label, const QString &text);

        QMenu *viewMenu;

        QTabWidget *tabWidget;
        Navigator *nav;
};

#endif /* GALICE_MAINWINDOW_HPP */
