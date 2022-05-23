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

#include <QtWidgets>
#include "file_manager.hpp"
#include "filesystem_view.hpp"
#include "mainwindow.hpp"
#include "navigator.hpp"
#include "navigator_model.hpp"
#include "navigator_view.hpp"

Navigator::Navigator(MainWindow *parent)
        : QDockWidget(tr("Navigation"), parent)
        , window(parent)
{
        fileSelector = new QComboBox;
        stack = new QStackedWidget;

        connect(fileSelector, QOverload<int>::of(&QComboBox::activated),
                stack, &QStackedWidget::setCurrentIndex);

        QWidget *widget = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(widget);
        layout->addWidget(fileSelector);
        layout->addWidget(stack);
        layout->setContentsMargins(0, 0, 0, 0);
        setWidget(widget);

        addFilesystem();

        connect(&FileManager::getInstance(), &FileManager::openedAinFile, this, &Navigator::addAinFile);
        connect(&FileManager::getInstance(), &FileManager::openedExFile, this, &Navigator::addExFile);
        connect(&FileManager::getInstance(), &FileManager::openedArchive, this, &Navigator::addArchive);
}

Navigator::~Navigator()
{
}

void Navigator::addFilesystem()
{
        QFileSystemModel *model = new QFileSystemModel;
        model->setRootPath(QDir::currentPath());

	FileSystemView *tree = new FileSystemView(model);
        tree->setRootIndex(model->index(QDir::currentPath()));

        for (int i = 1; i < model->columnCount(); i++) {
                tree->setColumnHidden(i, true);
        }
        tree->setHeaderHidden(true);

        addFile(tr("Filesystem"), tree);
}

void Navigator::addAinFile(const QString &fileName, std::shared_ptr<struct ain> ain)
{
        QWidget *view = new QWidget;
        NavigatorModel *classModel = NavigatorModel::fromAinClasses(ain);
        NavigatorModel *functionModel = NavigatorModel::fromAinFunctions(ain);

        QComboBox *viewSelector = new QComboBox;
        QStackedWidget *views = new QStackedWidget;
        connect(viewSelector, QOverload<int>::of(&QComboBox::activated),
                views, &QStackedWidget::setCurrentIndex);

        NavigatorView *classView = new NavigatorView(classModel);
        classView->setHeaderHidden(true);

        NavigatorView *functionView = new NavigatorView(functionModel);
        functionView->setHeaderHidden(true);

        for (int i = 1; i < classModel->columnCount(); i++) {
                classView->setColumnHidden(i, true);
        }
        for (int i = 1; i < functionModel->columnCount(); i++) {
                functionView->setColumnHidden(i, true);
        }

	connect(classView, &NavigatorView::requestedOpenClass, window, &MainWindow::openClass);
	connect(classView, &NavigatorView::requestedOpenFunction, window, &MainWindow::openFunction);
	connect(functionView, &NavigatorView::requestedOpenFunction, window, &MainWindow::openFunction);

        viewSelector->addItem(tr("Classes"));
        views->addWidget(classView);
        viewSelector->addItem(tr("Functions"));
        views->addWidget(functionView);

        QVBoxLayout *layout = new QVBoxLayout(view);
        layout->addWidget(viewSelector);
        layout->addWidget(views);
        layout->setContentsMargins(0, 0, 0, 0);

        addFile(fileName, view);
}

void Navigator::addExFile(const QString &fileName, std::shared_ptr<struct ex> ex)
{
        QWidget *widget = new QWidget;
        NavigatorModel *model = NavigatorModel::fromExFile(ex);
        NavigatorView *view = new NavigatorView(model);
        view->setColumnWidth(0, 150);
        view->setColumnWidth(1, 50);

	connect(view, &NavigatorView::requestedOpenExValue, window, &MainWindow::openExValue);

        QVBoxLayout *layout = new QVBoxLayout(widget);
        layout->addWidget(view);
        layout->setContentsMargins(0, 0, 0, 0);
        addFile(fileName, widget);
}

void Navigator::addArchive(const QString &fileName, std::shared_ptr<struct archive> ar)
{
        QWidget *widget = new QWidget;
        NavigatorModel *model = NavigatorModel::fromArchive(ar);
        NavigatorView *view = new NavigatorView(model);

        connect(view, &NavigatorView::requestedOpenArchiveFile, window, &MainWindow::openArchiveFile);
        connect(view, &NavigatorView::requestedOpenExValue, window, &MainWindow::openExValue);

        QVBoxLayout *layout = new QVBoxLayout(widget);
        layout->addWidget(view);
        layout->setContentsMargins(0, 0, 0, 0);
        addFile(fileName, widget);
}

void Navigator::addFile(const QString &name, QWidget *widget)
{
        int index = fileSelector->count();
        fileSelector->addItem(QFileInfo(name).fileName());
        stack->addWidget(widget);

        fileSelector->setCurrentIndex(index);
        stack->setCurrentIndex(index);
}
