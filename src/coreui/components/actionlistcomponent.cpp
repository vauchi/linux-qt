// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "actionlistcomponent.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QJsonArray>

QWidget *ActionListComponent::render(const QJsonObject &data,
                                     const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QString componentId = data["id"].toString();

    QJsonArray items = data["items"].toArray();
    for (const auto &item : items) {
        QJsonObject itemObj = item.toObject();
        auto *btn = new QPushButton(itemObj["label"].toString());

        if (onAction) {
            QString itemId = itemObj["id"].toString();
            QObject::connect(btn, &QPushButton::clicked, btn,
                             [onAction, componentId, itemId]() {
                                 QJsonObject action;
                                 QJsonObject inner;
                                 inner["component_id"] = componentId;
                                 inner["item_id"] = itemId;
                                 action["ListItemSelected"] = inner;
                                 onAction(action);
                             });
        }

        layout->addWidget(btn);
    }

    return container;
}
