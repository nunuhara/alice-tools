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
#include "system4/aar.h"
#include "system4/afa.h"
#include "system4/alk.h"
#include "system4/dlf.h"
#include "system4/flat.h"
#include "system4/ex.h"
#include "system4/cg.h"
#include "system4/file.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/acx.h"
#include "alice/ar.h"
}

FileFormat extensionToFileFormat(QString extension)
{
	if (!extension.compare("ain", Qt::CaseInsensitive))
		return FileFormat::AIN;
	if (!extension.compare("acx", Qt::CaseInsensitive))
		return FileFormat::ACX;
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
	if (!extension.compare("red", Qt::CaseInsensitive))
		return FileFormat::AAR;
	if (!extension.compare("afa", Qt::CaseInsensitive))
		return FileFormat::AFA;
	if (!extension.compare("ald", Qt::CaseInsensitive))
		return FileFormat::ALD;
	if (!extension.compare("alk", Qt::CaseInsensitive))
		return FileFormat::ALK;
	if (!extension.compare("dlf", Qt::CaseInsensitive))
		return FileFormat::DLF;
	if (!extension.compare("flat", Qt::CaseInsensitive))
		return FileFormat::FLAT;
	return FileFormat::NONE;
}

QString fileFormatToExtension(FileFormat format)
{
	switch (format) {
	case FileFormat::NONE:
		return "";
	case FileFormat::AIN:
		return "ain";
	case FileFormat::ACX:
		return "acx";
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
	case FileFormat::AAR:
		return "red";
	case FileFormat::AFA:
		return "afa";
	case FileFormat::ALD:
		return "ald";
	case FileFormat::ALK:
		return "alk";
	case FileFormat::DLF:
		return "dlf";
	case FileFormat::FLAT:
		return "flat";
	}
	return "";
}

bool isArchiveFormat(FileFormat format)
{
	switch (format) {
	case FileFormat::AAR:
	case FileFormat::AFA:
	case FileFormat::ALD:
	case FileFormat::ALK:
	case FileFormat::DLF:
	case FileFormat::FLAT:
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

FileManager::FileManager()
	: QObject()
{
}

FileManager::~FileManager()
{
}

void FileManager::openFile(const QString &path, bool newTab)
{
	QString suffix = QFileInfo(path).suffix();
	FileFormat format = extensionToFileFormat(QFileInfo(path).suffix());
	switch (format) {
	case FileFormat::NONE:
	// TODO
	case FileFormat::TXTEX:
	case FileFormat::JAF:
	case FileFormat::JAM:
		emit openFileError(path, tr("Unsupported file type"));
		break;
	case FileFormat::PNG:
	case FileFormat::WEBP:
	case FileFormat::QNT:
	case FileFormat::AJP:
	case FileFormat::DCF:
		openImageFile(path, newTab);
		break;
	case FileFormat::AIN:
		openAinFile(path);
		break;
	case FileFormat::ACX:
		openAcxFile(path, newTab);
		break;
	case FileFormat::EX:
		openExFile(path);
		break;
	case FileFormat::AAR:
	case FileFormat::AFA:
	case FileFormat::ALD:
	case FileFormat::ALK:
	case FileFormat::DLF:
	case FileFormat::FLAT:
		openArchive(path, format);
		break;
	}
}

void FileManager::openAinFile(const QString &path)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	int error = AIN_SUCCESS;
	set_encodings("CP932", "UTF-8");
	struct ain *ain = ain_open_conv(path.toUtf8(), conv_output, &error);
	if (!ain) {
		QGuiApplication::restoreOverrideCursor();
		emit openFileError(path, tr("Failed to read .ain file"));
		return;
	}
	// initialize method-struct mappings
	ain_init_member_functions(ain, strdup);

	std::shared_ptr<struct ain> ptr(ain, ain_free);
	emit openedAinFile(path, ptr);
	QGuiApplication::restoreOverrideCursor();
}

void FileManager::openExFile(const QString &path)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	set_encodings("CP932", "UTF-8");
	struct ex *ex = ex_read_file_conv(path.toUtf8(), string_conv_output);
	if (!ex) {
		QGuiApplication::restoreOverrideCursor();
		emit openFileError(path, tr("Failed to read .ex file"));
		return;
	}

	std::shared_ptr<struct ex> ptr(ex, ex_free);
	emit openedExFile(path, ptr);
	QGuiApplication::restoreOverrideCursor();
}

void FileManager::openAcxFile(const QString &path, bool newTab)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	int error = ACX_SUCCESS;
	set_encodings("CP932", "UTF-8");
	struct acx *acx = acx_load_conv(path.toUtf8(), &error, string_conv_output);
	if (!acx) {
		QGuiApplication::restoreOverrideCursor();
		emit openFileError(path, tr("Failed to read .acx file"));
		return;
	}

	std::shared_ptr<struct acx> ptr(acx, acx_free);
	emit openedAcxFile(path, ptr, newTab);
	QGuiApplication::restoreOverrideCursor();
}

static std::shared_ptr<struct archive> openArchiveFile(const QString &path, FileFormat format, int *error)
{
	struct archive *ar = nullptr;
	set_encodings("CP932", "UTF-8");
	switch (format) {
	case FileFormat::AAR: {
		struct aar_archive *aar = aar_open(path.toUtf8(), 0, error);
		if (aar)
			ar = &aar->ar;
		break;
	}
	case FileFormat::AFA: {
		struct afa_archive *afa = afa_open_conv(path.toUtf8(), 0, error, string_conv_output);
		if (afa)
			ar = &afa->ar;
		break;
	}
	case FileFormat::ALD: {
		ar = open_ald_archive(path.toUtf8(), error, conv_output);
		break;
	}
	case FileFormat::ALK: {
		struct alk_archive *alk = alk_open(path.toUtf8(), 0, error);
		if (alk)
			ar = &alk->ar;
		break;
	}
	case FileFormat::DLF: {
		struct dlf_archive *dlf = dlf_open(path.toUtf8(), 0, error);
		if (dlf)
			ar = &dlf->ar;
		break;
	}
	case FileFormat::FLAT: {
		struct flat_archive *flat = flat_open_file(path.toUtf8(), 0, error);
		if (flat)
			ar = &flat->ar;
		break;
	}
	default:
		break;
	}
	return std::shared_ptr<struct archive>(ar, archive_free);
}

void FileManager::openArchive(const QString &path, FileFormat format)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	int error = ARCHIVE_SUCCESS;
	std::shared_ptr<struct archive> ar = openArchiveFile(path, format, &error);
	if (!ar) {
		QGuiApplication::restoreOverrideCursor();
		emit openFileError(path, tr("Failed to read .%1 file").arg(fileFormatToExtension(format)));
		return;
	}

	emit openedArchive(path, ar);
	QGuiApplication::restoreOverrideCursor();
}

void FileManager::openImageFile(const QString &path, bool newTab)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	struct cg *cg = cg_load_file(path.toUtf8());
	if (!cg) {
		QGuiApplication::restoreOverrideCursor();
		emit openFileError(path, tr("Failed to read image file"));
		return;
	}

	emit openedImageFile(path, std::shared_ptr<struct cg>(cg, cg_free), newTab);
	QGuiApplication::restoreOverrideCursor();
}
