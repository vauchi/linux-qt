// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "togglelistcomponent.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QJsonArray>

QWidget *ToggleListComponent::render(const QJsonObject &data,
                                     const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    container->setObjectName(data["id"].toString());

    auto *label = new QLabel(data["label"].toString());
    layout->addWidget(label);

    QString componentId = data["id"].toString();

    QJsonArray items = data["items"].toArray();
    for (const auto &item : items) {
        QJsonObject itemObj = item.toObject();
        auto *checkbox = new QCheckBox(itemObj["label"].toString());
        checkbox->setAccessibleName(itemObj["label"].toString());
        checkbox->setChecked(itemObj["selected"].toBool());

        if (onAction) {
            QString itemId = itemObj["id"].toString();
            QObject::connect(checkbox, &QCheckBox::toggled, checkbox,
                             [onAction, componentId, itemId](bool /*checked*/) {
                                 QJsonObject action;
                                 QJsonObject inner;
                                 inner["component_id"] = componentId;
                                 inner["item_id"] = itemId;
                                 action["ItemToggled"] = inner;
                                 onAction(action);
                             });
        }

        layout->addWidget(checkbox);
    }

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) {
            container->setAccessibleName(a11yLabel);
        }
        auto hint = a11y.value("hint").toString();
        if (!hint.isEmpty()) {
            container->setAccessibleDescription(hint);
        }
    }

    return container;
}
