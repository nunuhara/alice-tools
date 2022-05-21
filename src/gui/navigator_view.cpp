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
	connect(this, &QTreeView::activated, this, &NavigatorView::requestOpen);
}

NavigatorView::~NavigatorView()
{
        delete model;
}

static NavigatorModel::NavigatorNode *getNode(const QModelIndex &index)
{
	return static_cast<const NavigatorModel*>(index.model())->getNode(index);
}

void NavigatorView::requestOpen(const QModelIndex &index) const
{
	if (!index.isValid())
		return;

	NavigatorModel::NavigatorNode *node = getNode(index);
	if (!node)
		return;

	openNode(node, false);
}

void NavigatorView::openNode(NavigatorModel::NavigatorNode *node, bool newTab) const
{
	switch (node->type) {
	case NavigatorModel::RootNode:
		break;
	case NavigatorModel::ClassNode:
		emit requestedOpenClass(node->ainItem.ainFile, node->ainItem.i, newTab);
		break;
	case NavigatorModel::FunctionNode:
		emit requestedOpenFunction(node->ainItem.ainFile, node->ainItem.i, newTab);
		break;
	case NavigatorModel::ExStringKeyValueNode:
		emit requestedOpenExValue(QString::fromUtf8(node->exKV.key.s), node->exKV.value, newTab);
		break;
	case NavigatorModel::ExIntKeyValueNode:
		emit requestedOpenExValue("[" + QString::number(node->exKV.key.i) + "]", node->exKV.value, newTab);
		break;
	case NavigatorModel::ExRowNode:
		// TODO
		break;
	case NavigatorModel::FileNode:
		switch (node->ar.type) {
		case NavigatorModel::NormalFile:
			emit requestedOpenArchiveFile(node->ar.file, newTab);
			break;
		case NavigatorModel::ExFile:
		case NavigatorModel::ArFile:
			break;
		}
		break;
	}
}

void NavigatorView::contextMenuEvent(QContextMenuEvent *event)
{
        // get selected index
        QModelIndexList selected = selectedIndexes();
        QModelIndex index = selected.isEmpty() ? QModelIndex() : selected.first();
        if (!index.isValid())
                return;

	NavigatorModel::NavigatorNode *node = getNode(index);
	if (!node)
		return;

        QMenu menu(this);
	menu.addAction(tr("Open"), [this, node]() -> void { this->openNode(node, false); });
	menu.addAction(tr("Open in New Tab"), [this, node]() -> void { this->openNode(node, true); });
        menu.exec(event->globalPos());
}
