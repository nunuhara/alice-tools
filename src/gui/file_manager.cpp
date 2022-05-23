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
#include <QFileInfo>
#include <QGuiApplication>
#include "file_manager.hpp"

extern "C" {
#include "system4/ain.h"
#include "system4/afa.h"
#include "system4/ex.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/acx.h"
#include "alice/ar.h"
}

FileFormat extensionToFileFormat(QString extension)
{
	if (!extension.compare("ain", Qt::CaseInsensitive))
		return FileFormat::AIN;
	if (!extension.compare("ex", Qt::CaseInsensitive))
		return FileFormat::EX;
	if (!extension.compare("pactex", Qt::CaseInsensitive))
		return FileFormat::EX;
	if (!extension.compare("txtex", Qt::CaseInsensitive))
		return FileFormat::TXTEX;
	if (!extension.compare("png", Qt::CaseInsensitive))
		return FileFormat::PNG;
	if (!extension.compare("webp", Qt::CaseInsensitive))
		return FileFormat::WEBP;
	if (!extension.compare("qnt", Qt::CaseInsensitive))
		return FileFormat::QNT;
	if (!extension.compare("ajp", Qt::CaseInsensitive))
		return FileFormat::AJP;
	if (!extension.compare("dcf", Qt::CaseInsensitive))
		return FileFormat::DCF;
	if (!extension.compare("jaf", Qt::CaseInsensitive))
		return FileFormat::JAF;
	if (!extension.compare("jam", Qt::CaseInsensitive))
		return FileFormat::JAM;
	if (!extension.compare("ald", Qt::CaseInsensitive))
		return FileFormat::ALD;
	if (!extension.compare("afa", Qt::CaseInsensitive))
		return FileFormat::AFA;
	if (!extension.compare("alk", Qt::CaseInsensitive))
		return FileFormat::ALK;
	if (!extension.compare("acx", Qt::CaseInsensitive))
		return FileFormat::ACX;
	return FileFormat::NONE;
}

QString fileFormatToExtension(FileFormat format)
{
	switch (format) {
	case FileFormat::NONE:
		return "";
	case FileFormat::AIN:
		return "ain";
	case FileFormat::EX:
		return "ex";
	case FileFormat::TXTEX:
		return "txtex";
	case FileFormat::PNG:
		return "png";
	case FileFormat::WEBP:
		return "webp";
	case FileFormat::QNT:
		return "qnt";
	case FileFormat::AJP:
		return "ajp";
	case FileFormat::DCF:
		return "dcf";
	case FileFormat::JAF:
		return "jaf";
	case FileFormat::JAM:
		return "jam";
	case FileFormat::ALD:
		return "ald";
	case FileFormat::AFA:
		return "afa";
	case FileFormat::ALK:
		return "alk";
	case FileFormat::ACX:
		return "acx";
	}
	return "";
}

bool isArchiveFormat(FileFormat format)
{
	switch (format) {
	case FileFormat::ALD:
	case FileFormat::AFA:
	case FileFormat::ALK:
		return true;
	default:
		return false;
	}
}

bool isImageFormat(FileFormat format)
{
	switch (format) {
	case FileFormat::PNG:
	case FileFormat::WEBP:
	case FileFormat::QNT:
	case FileFormat::AJP:
	case FileFormat::DCF:
		return true;
	default:
		return false;
	}
}

FileManager::AliceFile::~AliceFile()
{
        switch (type) {
        case Ain:
                ain_free(ain);
                break;
        case Ex:
                ex_free(ex);
                break;
        case Acx:
		acx_free(acx);
                break;
        case Archive:
                archive_free(ar);
                break;
        }
}

FileManager::FileManager()
        : QObject()
{
}

FileManager::~FileManager()
{
        qDeleteAll(files);
}

void FileManager::openFile(const QString &path)
{
        QString suffix = QFileInfo(path).suffix();
	switch (extensionToFileFormat(QFileInfo(path).suffix())) {
	case FileFormat::NONE:
	// TODO
	case FileFormat::TXTEX:
	case FileFormat::PNG:
	case FileFormat::WEBP:
	case FileFormat::QNT:
	case FileFormat::AJP:
	case FileFormat::DCF:
	case FileFormat::JAF:
	case FileFormat::JAM:
		emit openFileError(path, tr("Unsupported file type"));
		break;
	case FileFormat::AIN:
		openAinFile(path);
		break;
	case FileFormat::EX:
		openExFile(path);
		break;
	case FileFormat::ALD:
		openAldFile(path);
		break;
	case FileFormat::AFA:
		openAfaFile(path);
		break;
	case FileFormat::ALK:
		openAlkFile(path);
		break;
	case FileFormat::ACX:
		openAcxFile(path);
		break;
	}
}

void FileManager::openAinFile(const QString &path)
{
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);

        int error = AIN_SUCCESS;
        set_input_encoding("CP932");
        set_output_encoding("UTF-8");
        struct ain *ain = ain_open_conv(path.toUtf8(), conv_output, &error);
        if (!ain) {
                QGuiApplication::restoreOverrideCursor();
                emit openFileError(path, tr("Failed to read .ain file"));
                return;
        }
        // initialize method-struct mappings
        ain_init_member_functions(ain, strdup);

        files.append(new AliceFile(ain));
        emit openedAinFile(path, ain);
        QGuiApplication::restoreOverrideCursor();
}

void FileManager::openExFile(const QString &path)
{
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);

        set_input_encoding("CP932");
        set_output_encoding("UTF-8");
        struct ex *ex = ex_read_file_conv(path.toUtf8(), string_conv_output);
        if (!ex) {
                QGuiApplication::restoreOverrideCursor();
                emit openFileError(path, tr("Failed to read .ex file"));
                return;
        }

        files.append(new AliceFile(ex));
        emit openedExFile(path, ex);
        QGuiApplication::restoreOverrideCursor();
}

void FileManager::openAcxFile(const QString &path)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	set_input_encoding("CP932");
	set_output_encoding("UTF-8");

	int error = ACX_SUCCESS;
	struct acx *acx = acx_load_conv(path.toUtf8(), &error, string_conv_output);
	if (!acx) {
		QGuiApplication::restoreOverrideCursor();
		emit openFileError(path, tr("Failed to read .acx file"));
		return;
	}

	files.append(new AliceFile(acx));
	emit openedAcxFile(path, acx);
	QGuiApplication::restoreOverrideCursor();
}

void FileManager::openAfaFile(const QString &path)
{
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);

        set_input_encoding("CP932");
        set_output_encoding("UTF-8");

        int error = ARCHIVE_SUCCESS;
        struct afa_archive *ar = afa_open_conv(path.toUtf8(), 0, &error, string_conv_output);
        if (!ar) {
                QGuiApplication::restoreOverrideCursor();
                emit openFileError(path, tr("Failed to read .afa file"));
                return;
        }

        files.append(new AliceFile(&ar->ar));
        emit openedArchive(path, &ar->ar);
        QGuiApplication::restoreOverrideCursor();
}

void FileManager::openAldFile(const QString &path)
{
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);

        set_input_encoding("CP932");
        set_output_encoding("UTF-8");

        int error = ARCHIVE_SUCCESS;
        struct archive *ar = open_ald_archive(path.toUtf8(), &error, conv_output);
        if (!ar) {
                QGuiApplication::restoreOverrideCursor();
                emit openFileError(path, tr("Failed to read .ald file"));
                return;
        }

        files.append(new AliceFile(ar));
        emit openedArchive(path, ar);
        QGuiApplication::restoreOverrideCursor();
}

void FileManager::openAlkFile(const QString &path)
{
        emit openFileError(path, tr(".alk files not yet supported"));
}
