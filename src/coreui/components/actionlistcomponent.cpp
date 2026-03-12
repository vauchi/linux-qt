// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "actionlistcomponent.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QJsonArray>

QWidget *ActionListComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QJsonArray items = data["items"].toArray();
    for (const auto &item : items) {
        QJsonObject itemObj = item.toObject();
        auto *btn = new QPushButton(itemObj["label"].toString());
        layout->addWidget(btn);
    }

    // TODO: Connect action buttons to workflow
    return container;
}
