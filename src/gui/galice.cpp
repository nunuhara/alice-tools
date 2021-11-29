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

#include <iostream>
#include <QApplication>
#include <QCommandLineParser>

#include "alice.h"

#include "mainwindow.hpp"
#include "file_manager.hpp"

int main(int argc, char *argv[])
{
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
                FileManager::getInstance().openFile(parser.positionalArguments().first());
        w.show();
        return app.exec();
}
