// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusindicatorcomponent.h"
#include <QLabel>
#include <QHBoxLayout>

QWidget *StatusIndicatorComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    container->setObjectName(data["id"].toString());
    container->setAccessibleName(data["title"].toString());
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    // Status dot with color
    auto *dot = new QLabel(QStringLiteral("\u25CF"));
    QString status = data["status"].toString();
    if (status == "Success") {
        dot->setStyleSheet("color: #2ecc71; font-size: 14px;");
    } else if (status == "Failed") {
        dot->setStyleSheet("color: #e74c3c; font-size: 14px;");
    } else if (status == "Warning") {
        dot->setStyleSheet("color: #f39c12; font-size: 14px;");
    } else if (status == "InProgress") {
        dot->setStyleSheet("color: #3498db; font-size: 14px;");
    } else {
        dot->setStyleSheet("color: #95a5a6; font-size: 14px;");
    }

    auto *title = new QLabel(data["title"].toString());
    layout->addWidget(dot);
    layout->addWidget(title);

    // Optional detail text
    QString detail = data["detail"].toString();
    if (!detail.isEmpty()) {
        auto *detailLabel = new QLabel(detail);
        detailLabel->setStyleSheet("color: #888;");
        layout->addWidget(detailLabel);
    }

    layout->addStretch();

    return container;
}
