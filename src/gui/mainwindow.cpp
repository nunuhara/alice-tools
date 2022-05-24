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

#include <iostream>
#include <QtWidgets>
#include "mainwindow.hpp"
#include "acx_model.hpp"
#include "acx_view.hpp"
#include "ex_table_model.hpp"
#include "ex_table_view.hpp"
#include "file_manager.hpp"
#include "navigator.hpp"
#include "viewer.hpp"

extern "C" {
#include "system4/cg.h"
#include "system4/acx.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/ex.h"
#include "alice/port.h"
}

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
{
        createActions();
        createStatusBar();
        createDockWindows();

        readSettings();

        setupViewer();
        setCentralWidget(tabWidget);

        setUnifiedTitleAndToolBarOnMac(true);

        connect(&FileManager::getInstance(), &FileManager::openFileError, this, &MainWindow::openError);
	connect(&FileManager::getInstance(), &FileManager::openedAcxFile, this, &MainWindow::openAcxFile);
	connect(&FileManager::getInstance(), &FileManager::openedImageFile, this, &MainWindow::openImage);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
        event->accept();
}

void MainWindow::open()
{
        QString fileName = QFileDialog::getOpenFileName(this);
        if (!fileName.isEmpty())
                FileManager::getInstance().openFile(fileName);
}

void MainWindow::about()
{
        QMessageBox::about(this, tr("About alice-tools"),
                           tr("TODO"));
}

void MainWindow::createActions()
{
        QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

        const QIcon openIcon = QIcon::fromTheme("document-open");
        QAction *openAct = new QAction(openIcon, tr("&Open..."), this);
        openAct->setShortcuts(QKeySequence::Open);
        openAct->setStatusTip(tr("Open an existing file"));
        connect(openAct, &QAction::triggered, this, &MainWindow::open);
        fileMenu->addAction(openAct);

        viewMenu = menuBar()->addMenu(tr("&View"));

        menuBar()->addSeparator();

        QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

        const QIcon aboutIcon = QIcon::fromTheme("help-about");
        QAction *aboutAct = new QAction(aboutIcon, tr("&About"), this);
        aboutAct->setStatusTip(tr("About alice-tools"));
        connect(aboutAct, &QAction::triggered, this, &MainWindow::about);
        helpMenu->addAction(aboutAct);
}

void MainWindow::createStatusBar()
{
        statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createDockWindows()
{
        nav = new Navigator(this);
        nav->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        addDockWidget(Qt::LeftDockWidgetArea, nav);
        resizeDocks({nav}, {350}, Qt::Horizontal);
        viewMenu->addAction(nav->toggleViewAction());
}

void MainWindow::setupViewer()
{
        tabWidget = new QTabWidget;
        tabWidget->setMovable(true);
        tabWidget->setTabsClosable(true);

        connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
        // TODO: display welcome page
}

void MainWindow::closeTab(int index)
{
        QWidget *w = tabWidget->widget(index);
        tabWidget->removeTab(index);
        delete w;
}

void MainWindow::readSettings()
{
        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
        if (geometry.isEmpty()) {
                const QRect availableGeometry = screen()->availableGeometry();
                resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
                move((availableGeometry.width() - width()) / 2,
                     (availableGeometry.height() - height()) / 2);
        } else {
                restoreGeometry(geometry);
        }
}

void MainWindow::writeSettings()
{
        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        settings.setValue("geometry", saveGeometry());
}

void MainWindow::openError(const QString &fileName, const QString &message)
{
        QMessageBox::critical(this, "alice-tools", message, QMessageBox::Ok);
}

void MainWindow::openClass(struct ain *ainObj, int i, bool newTab)
{
        struct port port;
        port_buffer_init(&port);
        set_encodings("UTF-8", "UTF-8");
        ain_dump_structure(&port, ainObj, i);
        char *data = (char*)port_buffer_get(&port, NULL);
        openText(ainObj->structures[i].name, data, newTab);
        free(data);
}

void MainWindow::openFunction(struct ain *ainObj, int i, bool newTab)
{
        struct port port;
        port_buffer_init(&port);
        set_encodings("UTF-8", "UTF-8");
        _ain_disassemble_function(&port, ainObj, i, 0);
        char *data = (char*)port_buffer_get(&port, NULL);
        openText(ainObj->functions[i].name, data, newTab);
        free(data);
}

void MainWindow::openExValue(const QString &name, struct ex_value *value, bool newTab)
{
	if (value->type == EX_TABLE) {
		ExTableModel *model = new ExTableModel(value->t);
		ExTableView *view = new ExTableView(model);
		openViewer(name, view, newTab);
		return;
	}
        struct port port;
        port_buffer_init(&port);
        set_encodings("UTF-8", "UTF-8");
        ex_dump_value(&port, value);
        char *data = (char*)port_buffer_get(&port, NULL);
        openText(name, data, newTab);
        free(data);
}

void MainWindow::openAcxFile(const QString &name, std::shared_ptr<struct acx> acx, bool newTab)
{
	AcxModel *model = new AcxModel(acx);
	AcxView *view = new AcxView(model);
	openViewer(QFileInfo(name).fileName(), view, newTab);
}

void MainWindow::openImage(const QString &name, std::shared_ptr<struct cg> cg, bool newTab)
{
	QImage image((uchar*)cg->pixels, cg->metrics.w, cg->metrics.h,
			cg->metrics.w*4, QImage::Format_RGBA8888);

	QLabel *imageLabel = new QLabel;
	imageLabel->setPixmap(QPixmap::fromImage(image));

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(imageLabel);
	scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	openViewer(name, scrollArea, newTab);
}

void MainWindow::openArchiveFile(struct archive_data *file, bool newTab)
{
        if (!archive_load_file(file)) {
                openError(file->name, "Failed to open archived file");
                return;
        }

        if (cg_check_format(file->data) != ALCG_UNKNOWN) {
                struct cg *cg = cg_load_data(file);
                if (!cg) {
                        openError(file->name, "Failed to load CG file");
                        archive_release_file(file);
                        return;
                }
                openImage(file->name, std::shared_ptr<struct cg>(cg, cg_free), newTab);
                return;
        }
        // TODO: preview other file types

        statusBar()->showMessage("No preview available");
        archive_release_file(file);
}

void MainWindow::openText(const QString &label, const QString &text, bool newTab)
{
        QFont font;
        font.setFamily("Courier");
        font.setFixedPitch(true);
        font.setPointSize(10);

        QTextEdit *viewer = new QTextEdit;
        viewer->setFont(font);
        viewer->setPlainText(text);

        openViewer(label, viewer, newTab);
}

void MainWindow::openViewer(const QString &label, QWidget *view, bool newTab)
{
        if (newTab || tabWidget->currentIndex() < 0) {
                Viewer *tabContent = new Viewer(view);
                int index = tabWidget->currentIndex()+1;
                tabWidget->insertTab(index, tabContent, label);
                tabWidget->setCurrentIndex(index);
                return;
        }

        Viewer *tabContent = static_cast<Viewer*>(tabWidget->currentWidget());
        tabContent->setChild(view);
        tabWidget->setTabText(tabWidget->currentIndex(), label);
}
