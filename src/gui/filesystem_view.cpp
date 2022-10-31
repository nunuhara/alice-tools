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

#include <QContextMenuEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>

#include "galice.hpp"
#include "filesystem_view.hpp"

extern "C" {
#include "system4/archive.h"
#include "alice/ar.h"
}

FileSystemView::FileSystemView(QFileSystemModel *model, QWidget *parent)
	: QTreeView(parent)
	, model(model)
{
	setModel(model);
	connect(this, &QTreeView::activated, this, &FileSystemView::openFile);
}

FileSystemView::~FileSystemView()
{
	delete model;
}

void FileSystemView::open(const QModelIndex &index, bool newTab)
{
	if (model->isDir(index))
		return;
	GAlice::openFile(model->filePath(index), newTab);
}

void FileSystemView::openFile(const QModelIndex &index)
{
	open(index, false);
}

void FileSystemView::extract(const QModelIndex &index)
{
	// TODO: settings dialog (images only, raw, etc).
	struct archive *ar;
	enum archive_type type;
	int error;
	QByteArray u = model->filePath(index).toUtf8();

	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	ar = open_archive(u, &type, &error);
	QGuiApplication::restoreOverrideCursor();

	if (!ar) {
		QMessageBox::critical(this, "alice-tools", tr("Failed to load archive"), QMessageBox::Ok);
		return;
	}

	QString dir = QFileDialog::getExistingDirectory(this, tr("Select output directory"), "",
			QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!dir.isEmpty()) {
		QGuiApplication::setOverrideCursor(Qt::WaitCursor);
		QByteArray u = dir.toUtf8();
		ar_extract_all(ar, u, 0);
		QGuiApplication::restoreOverrideCursor();
	}
	archive_free(ar);
}

void FileSystemView::contextMenuEvent(QContextMenuEvent *event)
{
	// get selected index
	QModelIndexList selected = selectedIndexes();
	QModelIndex index = selected.isEmpty() ? QModelIndex() : selected.first();
	if (!index.isValid())
		return;

	FileFormat format = extensionToFileFormat(model->fileInfo(index).suffix());
	if (format == FileFormat::NONE)
		return;

	QMenu menu(this);
	menu.addAction(tr("Open"), [this, index]() -> void { this->open(index, false); });
	if (isImageFormat(format) || format == FileFormat::ACX) {
		menu.addAction(tr("Open in New Tab"), [this, index]() -> void { this->open(index, true); });
	}
	if (isArchiveFormat(format)) {
		menu.addAction(tr("Extract"), [this, index]() -> void { this->extract(index); });
	}
	if (isImageFormat(format)) {
		// TODO
		//menu.addAction(tr("Convert"), [this, index]() -> void { this->convert(index); });
	}

	menu.exec(event->globalPos());
}
