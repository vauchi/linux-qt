// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textcomponent.h"
#include <QLabel>

QWidget *TextComponent::render(const QJsonObject &data) {
    auto *label = new QLabel(data["content"].toString());
    label->setWordWrap(true);
    label->setObjectName(data["id"].toString());
    label->setAccessibleName(data["content"].toString());

    QString style = data["style"].toString();
    if (style == "Title") {
        label->setStyleSheet("font-size: 20px; font-weight: bold;");
    } else if (style == "Subtitle") {
        label->setStyleSheet("font-size: 16px; color: #666;");
    } else if (style == "Caption") {
        label->setStyleSheet("font-size: 12px; color: #888;");
    }
    // Body is the default — no extra styling needed

    return label;
}
