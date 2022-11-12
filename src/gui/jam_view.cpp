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

#include <QFont>
#include <QRegularExpression>
#include <QTextCharFormat>
#include "jam_view.hpp"

JamView::JamView(QWidget *parent)
	: QTextEdit(parent)
{
	QFont font;
	font.setFamily("Courier");
	font.setFixedPitch(true);
	font.setPointSize(10);
	setFont(font);

	highlighter = new SyntaxHighlighter(document());

	QTextCharFormat fmt;

	fmt.setForeground(Qt::blue);
	fmt.setFontWeight(QFont::Bold);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bFUNC\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bENDFUNC\\b")), fmt);
	fmt.setFontWeight(QFont::Normal);

	fmt.setForeground(Qt::darkCyan);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b0x[a-fA-F0-9]+\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b[1-9][0-9]*\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b0[0-7]*\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b[0-9]+\\.[0-9]+\\b")), fmt);

	fmt.setForeground(Qt::darkGray);
	highlighter->addRule(QRegularExpression(QStringLiteral("^\\S+:")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("^\\.CASE\\b")), fmt);

	fmt.setForeground(Qt::red);
	highlighter->addRule(QRegularExpression(QStringLiteral("\"(\\\\.|[^\"\\\\])*\"")), fmt);

	fmt.setForeground(Qt::darkGreen);
	highlighter->addRule(QRegularExpression(QStringLiteral(";[^\n]*")), fmt);

	setReadOnly(true);
}

JamView::JamView(const QString &text, QWidget *parent)
	: JamView(parent)
{
	setPlainText(text);
}
