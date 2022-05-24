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

#ifndef GALICE_FILE_MANAGER_HPP
#define GALICE_FILE_MANAGER_HPP

#include <memory>
#include <QObject>

struct ain;
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

class FileManager : public QObject
{
	Q_OBJECT

public:
	static FileManager& getInstance()
	{
		static FileManager instance;
		return instance;
	}
	FileManager(FileManager const&) = delete;
	void operator=(FileManager const&) = delete;

public slots:
	void openFile(const QString &path);

signals:
	void openedAinFile(const QString &fileName, std::shared_ptr<struct ain> ain);
	void openedExFile(const QString &fileName, std::shared_ptr<struct ex> ex);
	void openedAcxFile(const QString &filename, std::shared_ptr<struct acx> acx);
	void openedArchive(const QString &fileName, std::shared_ptr<struct archive> ar);
	void openFileError(const QString &fileName, const QString &message);

private:
	FileManager();
	~FileManager();
	void openAinFile(const QString &path);
	void openExFile(const QString &path);
	void openAcxFile(const QString &path);
	void openArchive(const QString &path, FileFormat format);
};

#endif /* GALICE_FILE_MANAGER_HPP */
