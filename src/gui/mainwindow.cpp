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
#include "ain_functions_model.hpp"
#include "file_manager.hpp"
#include "navigator.hpp"

extern "C" {
#include "alice/ain.h"
}

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
{
        createActions();
        createStatusBar();
        createDockWindows();

        readSettings();

        setupViewer();
        setCentralWidget(viewer);

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

        connect(nav, &Navigator::fileOpen, &FileManager::getInstance(), &FileManager::openFile);

        addDockWidget(Qt::LeftDockWidgetArea, nav);
        viewMenu->addAction(nav->toggleViewAction());
}

void MainWindow::setupViewer()
{
        QFont font;
        font.setFamily("Courier");
        font.setFixedPitch(true);
        font.setPointSize(10);

        viewer = new QTextEdit;
        viewer->setFont(font);

        // TODO: syntax highlighter
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
