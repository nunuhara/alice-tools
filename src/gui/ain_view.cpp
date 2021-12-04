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

#include <QComboBox>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include "ain_view.hpp"
#include "navigator_model.hpp"
#include "navigator_view.hpp"

extern "C" {
#include "system4/ain.h"
}

AinView::AinView(struct ain *ain, QWidget *parent)
        : QWidget(parent)
{
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

        connect(classModel, &NavigatorModel::requestedOpenClass, this, &AinView::requestedOpenClass);
        connect(classModel, &NavigatorModel::requestedOpenFunction, this, &AinView::requestedOpenFunction);
        connect(functionModel, &NavigatorModel::requestedOpenFunction, this, &AinView::requestedOpenFunction);

        viewSelector->addItem(tr("Classes"));
        views->addWidget(classView);
        viewSelector->addItem(tr("Functions"));
        views->addWidget(functionView);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(viewSelector);
        layout->addWidget(views);
        layout->setContentsMargins(0, 0, 0, 0);
}
