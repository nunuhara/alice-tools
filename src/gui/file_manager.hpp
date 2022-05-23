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

#include <QObject>

struct ain;
struct ex;
struct acx;
struct archive;

enum class FileFormat {
	NONE,
	AIN,
	EX,
	TXTEX,
	PNG,
	WEBP,
	QNT,
	AJP,
	DCF,
	JAF,
	JAM,
	ALD,
	AFA,
	ALK,
	ACX,
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
        void openedAinFile(const QString &fileName, struct ain *ain);
        void openedExFile(const QString &fileName, struct ex *ex);
	void openedAcxFile(const QString &filename, struct acx *acx);
        void openedArchive(const QString &fileName, struct archive *ar);
        void openFileError(const QString &fileName, const QString &message);

private:
        enum FileType {
                Ain,
                Ex,
                Acx,
                Archive,
        };
        class AliceFile {
        public:
                AliceFile(struct ain *ain) : type(Ain), ain(ain) {}
                AliceFile(struct ex *ex) : type(Ex), ex(ex) {}
		AliceFile(struct acx *acx) : type(Acx), acx(acx) {}
                AliceFile(struct archive *ar) : type(Archive), ar(ar) {}
                ~AliceFile();
                FileType type;
                union {
                        struct ain *ain;
                        struct ex *ex;
			struct acx *acx;
                        struct archive *ar;
                };
        };
        QVector<AliceFile*> files;

        FileManager();
        ~FileManager();
        void openAinFile(const QString &path);
        void openExFile(const QString &path);
        void openAcxFile(const QString &path);
        void openAfaFile(const QString &path);
        void openAldFile(const QString &path);
        void openAlkFile(const QString &path);
};

#endif /* GALICE_FILE_MANAGER_HPP */
