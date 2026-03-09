// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fieldlistcomponent.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonArray>

QWidget *FieldListComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QJsonArray fields = data["fields"].toArray();
    for (const auto &field : fields) {
        QJsonObject fieldObj = field.toObject();
        auto *row = new QLabel(
            fieldObj["label"].toString() + ": " + fieldObj["value"].toString()
        );
        layout->addWidget(row);
    }

    // TODO: Support field editing and reordering
    return container;
}
