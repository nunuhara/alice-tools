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

#include <cctype>
#include <iostream>
#include <QApplication>
#include <QCommandLineParser>
#include <QCursor>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>

#include "galice.hpp"
#include "mainwindow.hpp"

extern "C" {
#include "system4.h"
#include "system4/ain.h"
#include "system4/aar.h"
#include "system4/afa.h"
#include "system4/alk.h"
#include "system4/buffer.h"
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

static void libsys4_error_handler(const char *msg)
{
	GAlice::criticalError(msg);
}

int main(int argc, char *argv[])
{
	sys_error_handler = libsys4_error_handler;

        QApplication app(argc, argv);
        QCoreApplication::setOrganizationName("nunuhara");
        QCoreApplication::setApplicationName("alice-tools");
        QCoreApplication::setApplicationVersion(ALICE_TOOLS_VERSION);

        QCommandLineParser parser;
        parser.setApplicationDescription(QCoreApplication::applicationName());
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addPositionalArgument("file", "The file to open.");
        parser.process(app);

        MainWindow w;
        w.setWindowTitle("alice-tools");
        if (!parser.positionalArguments().isEmpty())
                GAlice::openFile(parser.positionalArguments().first());
        w.show();
        return app.exec();
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
	if (!extension.compare("pcf", Qt::CaseInsensitive))
		return FileFormat::PCF;
	if (!extension.compare("jaf", Qt::CaseInsensitive))
		return FileFormat::JAF;
	if (!extension.compare("hll", Qt::CaseInsensitive))
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
	case FileFormat::PCF:
		return "pcf";
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
	case FileFormat::PCF:
		return true;
	default:
		return false;
	}
}

GAlice::GAlice()
	: QObject()
{
}

GAlice::~GAlice()
{
}

void GAlice::openFile(const QString &path, bool newTab)
{
	QString suffix = QFileInfo(path).suffix();
	FileFormat format = extensionToFileFormat(QFileInfo(path).suffix());
	switch (format) {
	case FileFormat::NONE:
		status(tr("Unsupported file type"));
		break;
	case FileFormat::TXTEX:
	case FileFormat::JAF:
	case FileFormat::JAM: {
		size_t len;
		char *text = (char*)file_read(path.toUtf8(), &len);
		openText(path, text, format, newTab);
		free(text);
		break;
	}
	case FileFormat::PNG:
	case FileFormat::WEBP:
	case FileFormat::QNT:
	case FileFormat::AJP:
	case FileFormat::DCF:
	case FileFormat::PCF:
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

void GAlice::openAinFile(const QString &path)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	int error = AIN_SUCCESS;
	set_encodings("CP932", "UTF-8");
	struct ain *ain = ain_open_conv(path.toUtf8(), conv_output, &error);
	if (!ain) {
		QGuiApplication::restoreOverrideCursor();
		fileError(path, tr("Failed to read .ain file"));
		return;
	}
	// initialize method-struct mappings
	ain_init_member_functions(ain, strdup);

	std::shared_ptr<struct ain> ptr(ain, ain_free);
	emit getInstance().openedAinFile(path, ptr);
	QGuiApplication::restoreOverrideCursor();
}

void GAlice::openExFile(const QString &path)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	set_encodings("CP932", "UTF-8");
	struct ex *ex = ex_read_file_conv(path.toUtf8(), string_conv_output);
	if (!ex) {
		QGuiApplication::restoreOverrideCursor();
		fileError(path, tr("Failed to read .ex file"));
		return;
	}

	std::shared_ptr<struct ex> ptr(ex, ex_free);
	emit getInstance().openedExFile(path, ptr);
	QGuiApplication::restoreOverrideCursor();
}

void GAlice::openAcxFile(const QString &path, bool newTab)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	int error = ACX_SUCCESS;
	set_encodings("CP932", "UTF-8");
	struct acx *acx = acx_load_conv(path.toUtf8(), &error, string_conv_output);
	if (!acx) {
		QGuiApplication::restoreOverrideCursor();
		fileError(path, tr("Failed to read .acx file"));
		return;
	}

	std::shared_ptr<struct acx> ptr(acx, acx_free);
	emit getInstance().openedAcxFile(path, ptr, newTab);
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

void GAlice::openArchive(const QString &path, FileFormat format)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	int error = ARCHIVE_SUCCESS;
	std::shared_ptr<struct archive> ar = openArchiveFile(path, format, &error);
	if (!ar) {
		QGuiApplication::restoreOverrideCursor();
		fileError(path, tr("Failed to read .%1 file").arg(fileFormatToExtension(format)));
		return;
	}

	emit getInstance().openedArchive(path, ar);
	QGuiApplication::restoreOverrideCursor();
}

void GAlice::openImageFile(const QString &path, bool newTab)
{
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	struct cg *cg = cg_load_file(path.toUtf8());
	if (!cg) {
		QGuiApplication::restoreOverrideCursor();
		fileError(path, tr("Failed to read image file"));
		return;
	}

	emit getInstance().openedImageFile(path, std::shared_ptr<struct cg>(cg, cg_free), newTab);
	QGuiApplication::restoreOverrideCursor();
}

void GAlice::openArchiveData(struct archive_data *file, bool newTab)
{
	if (!archive_load_file(file)) {
		fileError(file->name, tr("Filed to load archived file"));
		return;
	}

	if (cg_check_format(file->data) != ALCG_UNKNOWN) {
		struct cg *cg = cg_load_data(file);
		if (!cg) {
			fileError(file->name, tr("Failed to load image"));
			archive_release_file(file);
			return;
		}
		emit getInstance().openedImageFile(file->name, std::shared_ptr<struct cg>(cg, cg_free), newTab);
	} else {
		openBinary(file->name, file->data, file->size, newTab);
		//status(tr("No preview available"));
	}
	archive_release_file(file);
}

void GAlice::openText(const QString &name, char *text, FileFormat format, bool newTab)
{
	emit getInstance().openedText(name, text, format, newTab);
}

void GAlice::openBinary(const QString &name, uint8_t *bytes, size_t size, bool newTab)
{
	struct buffer b;
	size_t hex_size = ((size / 16) + 3) * 76 + 1; // 76 chars per line, 16 bytes per line
	buffer_init(&b, (uint8_t*)xmalloc(hex_size), hex_size);

	buffer_write_cstring(&b, "Address  0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  ASCII             \n");
	buffer_write_cstring(&b, "-------- ----------------------------------------------- ------------------\n");

	for (unsigned addr = 0; addr < size; addr += 16) {
		// write address
		b.index += sprintf((char*)b.buf+b.index, "%08x ", addr);

		// write bytes (hex)
		unsigned i;
		for (i = addr; i < addr + 16 && i < size; i++) {
			b.index += sprintf((char*)b.buf+b.index, "%02hhx ", bytes[i]);
		}
		if (i == size) {
			unsigned remaining = 16 - (i - addr);
			memset(b.buf+b.index, ' ', remaining * 3);
			b.index += remaining * 3;
		}

		b.buf[b.index++] = '|';
		for (i = addr; i < addr + 16 && i < size; i++) {
			b.buf[b.index++] = isprint(bytes[i]) ? bytes[i] : '?';
		}
		if (i == size) {
			unsigned remaining = 16 - (i - addr);
			memset(b.buf+b.index, ' ', remaining);
			b.index += remaining;
		}
		b.buf[b.index++] = '|';
		b.buf[b.index++] = '\n';
	}

	b.buf[b.index++] = '\0';

	openText(name, (char*)b.buf, FileFormat::NONE, newTab);
	free(b.buf);
}

void GAlice::openAinFunction(struct ain *ain, int i, bool newTab)
{
	emit getInstance().openedAinFunction(ain, i, newTab);
}

void GAlice::openExValue(const QString &name, struct ex_value *value, bool newTab)
{
	emit getInstance().openedExValue(name, value, newTab);
}

void GAlice::fileError(const QString &filename, const QString &message)
{
	error(QString("%1: %2").arg(message).arg(filename));
}

void GAlice::error(const QString &message)
{
	emit getInstance().errorMessage(message);
}

void GAlice::status(const QString &message)
{
	emit getInstance().statusMessage(message);
}

void GAlice::criticalError(const QString &message)
{
	QMessageBox msgBox;
	msgBox.setText(message);
	msgBox.setInformativeText("The program will now exit.");
	msgBox.setIcon(QMessageBox::Critical);
	msgBox.exec();
	sys_exit(1);
}
