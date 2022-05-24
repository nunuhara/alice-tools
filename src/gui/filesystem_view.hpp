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

#ifndef GALICE_FILESYSTEM_VIEW_HPP
#define GALICE_FILESYSTEM_VIEW_HPP

#include <QTreeView>
#include <QFileSystemModel>

class FileSystemView : public QTreeView
{
	Q_OBJECT
public:
	FileSystemView(QFileSystemModel *model, QWidget *parent = nullptr);
	~FileSystemView();

private slots:
        void openFile(const QModelIndex &index);

protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
private:
	void open(const QModelIndex &index, bool newTab);
	void extract(const QModelIndex &index);
	QFileSystemModel *model;
};

#endif /* GALICE_FILESYSTEM_VIEW_HPP */
