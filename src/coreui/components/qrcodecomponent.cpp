// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qrcodecomponent.h"
#include <QLabel>
#include <QVBoxLayout>

QWidget *QrcodeComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *placeholder = new QLabel("[QR Code]");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setMinimumSize(200, 200);
    placeholder->setStyleSheet("border: 1px solid gray; background: white;");
    layout->addWidget(placeholder);

    // TODO: Render actual QR code from data["data"]
    return container;
}
