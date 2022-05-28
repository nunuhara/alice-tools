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

#ifndef GALICE_NAVIGATOR_VIEW_HPP
#define GALICE_NAVIGATOR_VIEW_HPP

#include <QTreeView>
#include "navigator_model.hpp"

class NavigatorView : public QTreeView
{
	Q_OBJECT
public:
	NavigatorView(NavigatorModel *model, QWidget *parent = nullptr);
	~NavigatorView();

private slots:
	void requestOpen(const QModelIndex &index) const;

protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
private:
	void exportNode(NavigatorNode *node);
	NavigatorModel *model;
};

#endif /* GALICE_NAVIGATOR_VIEW_HPP */
