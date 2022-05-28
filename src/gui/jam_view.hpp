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

#ifndef GALICE_JAM_HIGHLIGHTER_HPP
#define GALICE_JAM_HIGHLIGHTER_HPP

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextEdit>

class JamHighlighter : public QSyntaxHighlighter {
	Q_OBJECT
public:
	JamHighlighter(QTextDocument *parent = nullptr);

protected:
	void highlightBlock(const QString &text) override;

private:
	struct HighlightingRule {
		QRegularExpression pattern;
		QTextCharFormat format;
	};
	QVector<HighlightingRule> highlightingRules;

	QTextCharFormat keywordFormat;
	QTextCharFormat numberFormat;
	QTextCharFormat labelFormat;
	QTextCharFormat stringFormat;
	QTextCharFormat commentFormat;
};

class JamView : public QTextEdit {
	Q_OBJECT
public:
	JamView(QWidget *parent = nullptr);
	JamView(const QString &text, QWidget *parent = nullptr);

private:
	JamHighlighter *highlighter;
};

#endif /* GALICE_JAM_HIGHLIGHTER_HPP */
