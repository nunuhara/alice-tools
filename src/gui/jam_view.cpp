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

#include "jam_view.hpp"

JamView::JamView(QWidget *parent)
	: QTextEdit(parent)
{
	QFont font;
	font.setFamily("Courier");
	font.setFixedPitch(true);
	font.setPointSize(10);
	setFont(font);

	highlighter = new JamHighlighter(document());
}

JamView::JamView(const QString &text, QWidget *parent)
	: JamView(parent)
{
	setPlainText(text);
}

JamHighlighter::JamHighlighter(QTextDocument *parent)
	: QSyntaxHighlighter(parent)
{
	HighlightingRule rule;

	keywordFormat.setForeground(Qt::blue);
	keywordFormat.setFontWeight(QFont::Bold);
	const QString keywordPatterns[] = {
		QStringLiteral("\\bFUNC\\b"),
		QStringLiteral("\\bENDFUNC\\b")
	};
	for (const QString &pattern : keywordPatterns) {
		rule.format = keywordFormat;
		rule.pattern = QRegularExpression(pattern);
		highlightingRules.append(rule);
	}

	numberFormat.setForeground(Qt::darkCyan);
	rule.format = numberFormat;
	rule.pattern = QRegularExpression(QStringLiteral("\\b0x[a-fA-F0-9]+\\b"));
	highlightingRules.append(rule); // hex
	rule.pattern = QRegularExpression(QStringLiteral("\\b[1-9][0-9]*\\b"));
	highlightingRules.append(rule); // decimal
	rule.pattern = QRegularExpression(QStringLiteral("\\b0[0-7]*\\b"));
	highlightingRules.append(rule); // octal
	rule.pattern = QRegularExpression(QStringLiteral("\\b[0-9]+\\.[0-9]+\\b"));
	highlightingRules.append(rule); // float

	labelFormat.setForeground(Qt::darkGray);
	rule.format = labelFormat;
	rule.pattern = QRegularExpression(QStringLiteral("^\\S+:"));
	highlightingRules.append(rule);
	rule.pattern = QRegularExpression(QStringLiteral("^\\.CASE\\b"));
	highlightingRules.append(rule);

	stringFormat.setForeground(Qt::red);
	rule.format = stringFormat;
	rule.pattern = QRegularExpression(QStringLiteral("\"(\\\\.|[^\"\\\\])*\""));
	highlightingRules.append(rule);

	commentFormat.setForeground(Qt::darkGreen);
	rule.format = commentFormat;
	rule.pattern = QRegularExpression(QStringLiteral(";[^\n]*"));
	highlightingRules.append(rule);
}

void JamHighlighter::highlightBlock(const QString &text)
{
	for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
		QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
		while (matchIterator.hasNext()) {
			QRegularExpressionMatch match = matchIterator.next();
			setFormat(match.capturedStart(), match.capturedLength(), rule.format);
		}
	}
}
