/* Copyright (C) 2023 Nunuhara Cabbage <nunuhara@haniwa.technology>
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

#include <QtWidgets>

#include "sjisdecoder.hpp"

extern "C" {
#include "system4.h"
#include "system4/utfsjis.h"
}

static unsigned char decodeNibble(char16_t c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	return 0;
}

static QString decode(QString input)
{
	input.remove(QRegularExpression("[ \r\n\t]"));
	qsizetype len = input.length();

	// validate
	if (len % 2 != 0) {
		return "<invalid input: odd number of digits>";
	}
	for (QChar c : input) {
		if (c >= '0' && c <= '9')
			continue;
		if (c >= 'a' && c <= 'f')
			continue;
		if (c >= 'A' && c <= 'F')
			continue;
		return "<invalid input: invalid digit>";
	}

	// decode hex -> SJIS
	char *sjis = (char*)malloc((len/2) + 1);
	for (int src = 0, dst = 0; src < len; src += 2, dst++) {
		unsigned char fst = decodeNibble(input[src].unicode());
		unsigned char snd = decodeNibble(input[src+1].unicode());
		sjis[dst] = (fst << 4) | snd;
	}
	sjis[len/2] = '\0';

	// decode SJIS -> QString
	char *u = sjis2utf(sjis, 0);
	QString r(u);
	free(u);
	free(sjis);
	return r;
}

SjisDecoder::SjisDecoder(QWidget *parent)
	: QDialog(parent)
{
	setMinimumWidth(400);
	inputEdit = new QLineEdit();
	inputEdit->setPlaceholderText("Enter hexadecimal-coded Shift-JIS text here.");
	outputEdit = new QLineEdit();
	outputEdit->setPlaceholderText("Click \"Decode\" to decode the input.");

	QFormLayout *formLayout = new QFormLayout;
	formLayout->addRow(tr("Input:"), inputEdit);
	formLayout->addRow(tr("Output:"), outputEdit);

	QWidget *form = new QWidget;
	form->setLayout(formLayout);

	QDialogButtonBox *buttonBox = new QDialogButtonBox;
	QPushButton *button = buttonBox->addButton("Decode", QDialogButtonBox::ActionRole);
	connect(button, &QAbstractButton::clicked, [this](bool checked) {
		outputEdit->setText(decode(inputEdit->text()));
	});

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(form);
	layout->addWidget(buttonBox);
	setLayout(layout);

	setWindowTitle("Shift-JIS Decoder");
}
