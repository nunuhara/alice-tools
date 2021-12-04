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

#include <QMenu>
#include <QContextMenuEvent>
#include "navigator_view.hpp"

NavigatorView::NavigatorView(NavigatorModel *model, QWidget *parent)
        : QTreeView(parent)
        , model(model)
{
        setModel(model);
        connect(this, &QTreeView::activated, model, &NavigatorModel::requestOpen);
}

NavigatorView::~NavigatorView()
{
        delete model;
}

// NOTE: This is a completely braindead construct to allow passing the CONTEXT
//       when executing actions for a CONTEXT menu. I would like to think there
//       is a better way but if so I could not find it.
struct ContextFunctor {
        enum ActionType { Open, OpenNew };
        ContextFunctor(const QModelIndex &index, ActionType action)
                : index(index), action(action) {}
        void operator()() {
                switch (action) {
                case Open:    static_cast<const NavigatorModel*>(index.model())->requestOpen(index); break;
                case OpenNew: static_cast<const NavigatorModel*>(index.model())->requestOpenNewTab(index); break;
                }
        }
private:
        const QModelIndex &index;
        ActionType action;
};

void NavigatorView::contextMenuEvent(QContextMenuEvent *event)
{
        // get selected index
        QModelIndexList selected = selectedIndexes();
        QModelIndex index = selected.isEmpty() ? QModelIndex() : selected.first();
        if (!index.isValid())
                return;

        QMenu menu(this);
        menu.addAction(tr("Open"), ContextFunctor(index, ContextFunctor::Open));
        menu.addAction(tr("Open in New Tab"), ContextFunctor(index, ContextFunctor::OpenNew));
        menu.exec(event->globalPos());
}
