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
#include "jaf_view.hpp"

JafView::JafView(QWidget *parent)
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
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bvoid\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bint\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bfloat\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bbool\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\blint\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bstring\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\barray\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bdelegate\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bref\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bimplements\\b")), fmt);

	fmt.setForeground(Qt::blue);
	fmt.setFontWeight(QFont::Bold);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bclass\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bstruct\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\benum\\b")), fmt);
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
	highlighter->addRule(QRegularExpression(QStringLiteral("//[^\n]*")), fmt);

	setReadOnly(true);
}

JafView::JafView(const QString &text, QWidget *parent)
	: JafView(parent)
{
	setPlainText(text);
}
