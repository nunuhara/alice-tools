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
#include "file_manager.hpp"
#include "navigator.hpp"
#include "viewer.hpp"

extern "C" {
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

        connect(nav, &Navigator::requestedOpenFile, &FileManager::getInstance(), &FileManager::openFile);
        connect(nav, &Navigator::requestedOpenClass, this, &MainWindow::openClass);
        connect(nav, &Navigator::requestedOpenFunction, this, &MainWindow::openFunction);
        connect(nav, &Navigator::requestedOpenExValue, this, &MainWindow::openExValue);

        addDockWidget(Qt::LeftDockWidgetArea, nav);
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
        set_input_encoding("UTF-8");
        set_output_encoding("UTF-8");
        ain_dump_structure(&port, ainObj, i);
        char *data = (char*)port_buffer_get(&port, NULL);
        openText(ainObj->structures[i].name, data, newTab);
        free(data);
}

void MainWindow::openFunction(struct ain *ainObj, int i, bool newTab)
{
        struct port port;
        port_buffer_init(&port);
        set_input_encoding("UTF-8");
        set_output_encoding("UTF-8");
        _ain_disassemble_function(&port, ainObj, i, 0);
        char *data = (char*)port_buffer_get(&port, NULL);
        openText(ainObj->functions[i].name, data, newTab);
        free(data);
}

void MainWindow::openExValue(const QString &name, struct ex_value *value, bool newTab)
{
        struct port port;
        port_buffer_init(&port);
        set_input_encoding("UTF-8");
        set_output_encoding("UTF-8");
        ex_dump_value(&port, value);
        char *data = (char*)port_buffer_get(&port, NULL);
        openText(name, data, newTab);
        free(data);
}

void MainWindow::openText(const QString &label, const QString &text, bool newTab)
{
        if (newTab || tabWidget->currentIndex() < 0) {
                openTextNew(label, text);
                return;
        }

        Viewer *tabContent = static_cast<Viewer*>(tabWidget->currentWidget());
        tabContent->setChild(createText(text));
        tabWidget->setTabText(tabWidget->currentIndex(), label);
}

void MainWindow::openTextNew(const QString &label, const QString &text)
{
        Viewer *tabContent = new Viewer(createText(text));

        int index = tabWidget->currentIndex()+1;
        tabWidget->insertTab(index, tabContent, label);
        tabWidget->setCurrentIndex(index);
}

QWidget *MainWindow::createText(const QString &text)
{
        QFont font;
        font.setFamily("Courier");
        font.setFixedPitch(true);
        font.setPointSize(10);

        QTextEdit *viewer = new QTextEdit;
        viewer->setFont(font);
        viewer->setPlainText(text);

        return viewer;
}
