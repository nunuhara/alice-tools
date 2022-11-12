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
#include "ex_view.hpp"

ExView::ExView(QWidget *parent)
	: QTextEdit(parent)
{
	QFont font;
	font.setFamily("Courier");
	font.setFixedPitch(true);
	font.setPointSize(10);
	setFont(font);
	setTabStopDistance(40);

	highlighter = new SyntaxHighlighter(document());

	QTextCharFormat fmt;

	fmt.setForeground(Qt::blue);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bint\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bfloat\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bstring\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\btable\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\blist\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\btree\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\bindexed\\b")), fmt);

	fmt.setForeground(Qt::darkCyan);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b0x[a-fA-F0-9]+\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b[1-9][0-9]*\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b0[0-7]*\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b[0-9]+\\.[0-9]+\\b")), fmt);

	fmt.setForeground(Qt::red);
	highlighter->addRule(QRegularExpression(QStringLiteral("\"(\\\\.|[^\"\\\\])*\"")), fmt);

	fmt.setForeground(Qt::darkGreen);
	highlighter->addRule(QRegularExpression(QStringLiteral("//[^\n]*")), fmt);

	setReadOnly(true);
}

ExView::ExView(const QString &text, QWidget *parent)
	: ExView(parent)
{
	setPlainText(text);
}
