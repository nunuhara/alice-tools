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

#include <QCursor>
#include <QMenu>
#include <QContextMenuEvent>
#include "ex_view.hpp"
#include "ex_model.hpp"

ExView::ExView(struct ex *exFile)
{
        model = new ExModel(exFile);
        setModel(model);
        connect(this, &QTreeView::activated, this, &ExView::activate);
}

ExView::~ExView()
{
        delete model;
}

void ExView::activate(const QModelIndex &index)
{
        struct ex_value *v = model->exValue(index);
        if (v)
                emit opened(model->data(index, Qt::DisplayRole).toString(), v, false);
}

// NOTE: This is a completely braindead construct to allow passing the CONTEXT
//       when executing actions for a CONTEXT menu. I would like to think there
//       is a better way but if so I could not find it.
struct ExContextFunctor {
        enum ExAction { OPEN, OPEN_NEW, };
        ExContextFunctor(ExView *view, const QString &name, struct ex_value *value, ExAction action)
                : view(view), name(name), value(value), action(action) {}
        void operator()() {
                switch (action) {
                case OPEN:     emit view->opened(name, value, false); break;
                case OPEN_NEW: emit view->opened(name, value, true); break;
                }
        }
private:
        ExView *view;
        const QString &name;
        struct ex_value *value;
        ExAction action;
};

void ExView::contextMenuEvent(QContextMenuEvent *event)
{
        QMenu menu(this);

        // get selected index
        QModelIndexList selected = selectedIndexes();
        QModelIndex index = selected.isEmpty() ? QModelIndex() : selected.first();

        // get name/value at selected index
        struct ex_value *v = model->exValue(index);
        QString name = model->exName(index);
        if (v == nullptr)
                return;

        menu.addAction(tr("Open"), ExContextFunctor(this, name, v, ExContextFunctor::OPEN));
        menu.addAction(tr("Open in New Tab"), ExContextFunctor(this, name, v, ExContextFunctor::OPEN_NEW));
        menu.exec(event->globalPos());
}
