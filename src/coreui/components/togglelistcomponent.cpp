// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "togglelistcomponent.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QJsonArray>

QWidget *ToggleListComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *label = new QLabel(data["label"].toString());
    layout->addWidget(label);

    QJsonArray items = data["items"].toArray();
    for (const auto &item : items) {
        QJsonObject itemObj = item.toObject();
        auto *checkbox = new QCheckBox(itemObj["label"].toString());
        checkbox->setChecked(itemObj["selected"].toBool());
        layout->addWidget(checkbox);
    }

    // TODO: Emit toggle changes back to workflow
    return container;
}
