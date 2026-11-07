// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QCheckBox>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionButton>

class PresentationCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;

protected:
    void paintEvent(QPaintEvent *event) override {
        QCheckBox::paintEvent(event);
        QStyleOptionButton option;
        initStyleOption(&option);
        const auto indicator = style()->subElementRect(
            QStyle::SE_CheckBoxIndicator, &option, this);
        const QRectF box = QRectF(indicator).adjusted(1, 1, -1, -1);
        const auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(palette().color(group, isChecked() ? QPalette::Highlight : QPalette::Base));
        painter.setPen(QPen(palette().color(group, QPalette::Text), 2));
        painter.drawRoundedRect(box, 3, 3);
        if (isChecked()) {
            QPainterPath check;
            check.moveTo(box.left() + box.width() * .22, box.top() + box.height() * .51);
            check.lineTo(box.left() + box.width() * .43, box.top() + box.height() * .72);
            check.lineTo(box.left() + box.width() * .80, box.top() + box.height() * .28);
            painter.setPen(QPen(palette().color(group, QPalette::HighlightedText),
                2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(check);
        }
    }
};
