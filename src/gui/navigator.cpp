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
#include "file_manager.hpp"
#include "navigator.hpp"
#include "ain_functions_model.hpp"
#include "ain_objects_model.hpp"
#include "ex_model.hpp"

Navigator::Navigator(QWidget *parent)
        : QDockWidget(tr("Navigation"), parent)
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
        //connect(&FileManager::getInstance(), &FileManager::openedArchive, this, &Navigator::addArchive);
}

Navigator::~Navigator()
{
        qDeleteAll(models);
}

void Navigator::addFilesystem()
{
        QFileSystemModel *model = new QFileSystemModel;
        model->setRootPath(QDir::currentPath());

        QTreeView *tree = new QTreeView;
        tree->setModel(model);
        tree->setRootIndex(model->index(QDir::currentPath()));

        connect(tree, &QTreeView::doubleClicked, this, &Navigator::filesystemOpen);

        addFile(tr("Filesystem"), tree);
}

void Navigator::filesystemOpen(const QModelIndex &index)
{
        const QFileSystemModel *model = static_cast<const QFileSystemModel*>(index.model());
        emit fileOpen(model->filePath(index));
}

void Navigator::addAinFile(const QString &fileName, struct ain *ain)
{
        QComboBox *viewSelector = new QComboBox;
        viewSelector->addItem(tr("Classes"));
        //viewSelector->addItem(tr("Enumerations"));
        //viewSelector->addItem(tr("Files"));
        viewSelector->addItem(tr("Functions"));
        //viewSelector->addItem(tr("Libraries"));

        QStackedWidget *views = new QStackedWidget;

        connect(viewSelector, QOverload<int>::of(&QComboBox::activated),
                views, &QStackedWidget::setCurrentIndex);

        QTreeView *classes = new QTreeView;
        AinObjectsModel *obj_model = new AinObjectsModel(ain, classes);
        classes->setModel(obj_model);
        classes->setHeaderHidden(true);
        models.append(obj_model);

        QListView *functions = new QListView;
        AinFunctionsModel *func_model = new AinFunctionsModel(ain, functions);
        functions->setModel(func_model);
        models.append(func_model);

        // double clicking an item opens it in the viewer
        // FIXME: double clicking also opens/closes nodes in tree view...
        connect(classes, &QTreeView::doubleClicked, obj_model, &AinObjectsModel::open);
        connect(functions, &QListView::doubleClicked, func_model, &AinFunctionsModel::open);

        // pass signals from model along to be handled by MainWindow
        connect(obj_model, &AinObjectsModel::openClass, this, &Navigator::openClass);
        connect(obj_model, &AinObjectsModel::openFunction, this, &Navigator::openFunction);
        connect(func_model, &AinFunctionsModel::openFunction, this, &Navigator::openFunction);

        views->addWidget(classes);
        views->addWidget(functions);

        QWidget *widget = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(widget);
        layout->addWidget(viewSelector);
        layout->addWidget(views);
        layout->setContentsMargins(0, 0, 0, 0);

        addFile(fileName, widget);
}

void Navigator::addExFile(const QString &fileName, struct ex *ex)
{
        QTreeView *view = new QTreeView;
        ExModel *model = new ExModel(ex);
        view->setModel(model);
        //view->setHeaderHidden(true);
        models.append(model);

        connect(view, &QTreeView::doubleClicked, model, &ExModel::open);
        connect(model, &ExModel::openExValue, this, &Navigator::openExValue);

        addFile(fileName, view);
}

//void Navigator::addArchive(const QString &fileName, struct archive *ar)
//{
//}

void Navigator::addFile(const QString &name, QWidget *widget)
{
        int index = fileSelector->count();
        fileSelector->addItem(QFileInfo(name).fileName());
        stack->addWidget(widget);

        fileSelector->setCurrentIndex(index);
        stack->setCurrentIndex(index);
}
