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

#include <memory>
#include <QMainWindow>
#include <QFileInfo>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTabWidget>
#include "galice.hpp"
#include "navigator.hpp"

struct ain;
struct ex_value;
struct acx;

class SjisDecoder;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

protected:
        void closeEvent(QCloseEvent *event) override;

public slots:
	void openTextFile(const QString &name, char *text, FileFormat format, bool newTab);
        void openFunction(struct ain *ain, int i, bool newTab);
        void openExValue(const QString &name, struct ex_value *value, bool newTab);
	void openAcxFile(const QString &name, std::shared_ptr<struct acx> acx, bool newTab);
	void openImage(const QString &name, std::shared_ptr<struct cg> cg, bool newTab);

private slots:
        void open();
        void about();
        void error(const QString &message);
	void status(const QString &message);
        void closeTab(int index);

private:
        void createActions();
        void createStatusBar();
        void createDockWindows();
        void readSettings();
        void writeSettings();
        void setupViewer();

        void openText(const QString &label, const QString &text, bool newTab);
        void openViewer(const QString &label, QWidget *view, bool newTab);

        QMenu *viewMenu;

        QTabWidget *tabWidget;
        Navigator *nav;

	SjisDecoder *decodeDialog = nullptr;
};

#endif /* GALICE_MAINWINDOW_HPP */
