// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textcomponent.h"
#include <QLabel>

QWidget *TextComponent::render(const QJsonObject &data) {
    auto *label = new QLabel(data["content"].toString());
    // TODO: Apply style based on data["style"]
    return label;
}
