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
struct archive;

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
        void openedArchive(const QString &fileName, struct archive *ar);
        void openFileError(const QString &fileName, const QString &message);

private:
        enum FileType {
                FILE_TYPE_AIN,
                FILE_TYPE_EX,
                FILE_TYPE_ACX,
                FILE_TYPE_ARCHIVE,
        };
        class AliceFile {
        public:
                AliceFile(struct ain *ain) : type(FILE_TYPE_AIN), ain(ain) {}
                AliceFile(struct ex *ex) : type(FILE_TYPE_EX), ex(ex) {}
                ~AliceFile();
                enum FileType type;
                union {
                        struct ain *ain;
                        struct ex *ex;
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
