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

#ifndef GALICE_GALICE_HPP
#define GALICE_GALICE_HPP

#include <memory>
#include <QObject>

struct ain;
struct cg;
struct ex;
struct acx;
struct archive;

enum class FileFormat {
	NONE,
	AIN,
	ACX,
	EX,
	TXTEX,
	PNG,
	WEBP,
	QNT,
	AJP,
	DCF,
	PCF,
	JAF,
	JAM,
	AAR,
	AFA,
	ALD,
	ALK,
	DLF,
	FLAT,
};

FileFormat extensionToFileFormat(QString extension);
QString fileFormatToExtension(FileFormat format);
bool isArchiveFormat(FileFormat format);
bool isImageFormat(FileFormat format);

class GAlice : public QObject
{
	Q_OBJECT

public:
	static GAlice& getInstance()
	{
		static GAlice instance;
		return instance;
	}
	GAlice(GAlice const&) = delete;
	void operator=(GAlice const&) = delete;

	static void openFile(const QString &path, bool newTab = false);
	static void openArchiveData(struct archive_data *file, bool newTab = false);
	static void openText(const QString &name, char *text, FileFormat format, bool newTab = false);
	static void openBinary(const QString &name, uint8_t *bytes, size_t size, bool newTab = false);
	static void openAinFunction(struct ain *ain, int i, bool newTab = false);
	static void openExValue(const QString &name, struct ex_value *value, bool newTab = false);
	static void error(const QString &message);
	[[noreturn]] static void criticalError(const QString &message);
	static void status(const QString &message);

signals:
	void openedAinFile(const QString &fileName, std::shared_ptr<struct ain> ain);
	void openedExFile(const QString &fileName, std::shared_ptr<struct ex> ex);
	void openedAcxFile(const QString &filename, std::shared_ptr<struct acx> acx, bool newTab);
	void openedArchive(const QString &fileName, std::shared_ptr<struct archive> ar);
	void openedImageFile(const QString &fileName, std::shared_ptr<struct cg> cg, bool newTab);
	void openedText(const QString &name, char *text, FileFormat format, bool newTab);
	void openedAinFunction(struct ain *ainFile, int i, bool newTab);
	void openedExValue(const QString &name, struct ex_value *value, bool newTab);
	void errorMessage(const QString &message);
	void statusMessage(const QString &message);

private:
	GAlice();
	~GAlice();
	static void openAinFile(const QString &path);
	static void openExFile(const QString &path);
	static void openAcxFile(const QString &path, bool newTab);
	static void openArchive(const QString &path, FileFormat format);
	static void openImageFile(const QString &path, bool newTab);
	static void fileError(const QString &filename, const QString &message);
};

#endif /* GALICE_GALICE_HPP */
