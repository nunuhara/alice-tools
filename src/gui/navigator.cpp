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
#include "navigator.hpp"
#include "ain_view.hpp"
#include "ex_view.hpp"

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
        emit requestedOpenFile(model->filePath(index));
}

void Navigator::addAinFile(const QString &fileName, struct ain *ain)
{
        AinView *view = new AinView(ain);
        connect(view, &AinView::requestedOpenClass, this, &Navigator::requestedOpenClass);
        connect(view, &AinView::requestedOpenFunction, this, &Navigator::requestedOpenFunction);
        addFile(fileName, view);
}

void Navigator::addExFile(const QString &fileName, struct ex *ex)
{
        ExView *view = new ExView(ex);
        connect(view, &ExView::requestedOpenExValue, this, &Navigator::requestedOpenExValue);
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
