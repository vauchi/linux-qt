// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsgroupcomponent.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QJsonArray>

QWidget *SettingsGroupComponent::render(const QJsonObject &data,
                                        const OnAction &onAction) {
    auto *group = new QGroupBox(data["label"].toString());
    group->setObjectName(data["id"].toString());
    group->setAccessibleName(data["label"].toString());
    auto *layout = new QVBoxLayout(group);

    QString componentId = data["id"].toString();

    QJsonArray items = data["items"].toArray();
    for (const auto &item : items) {
        QJsonObject itemObj = item.toObject();
        QString itemId = itemObj["id"].toString();
        QString itemLabel = itemObj["label"].toString();
        QJsonObject kind = itemObj["kind"].toObject();

        if (kind.contains("Toggle")) {
            // Toggle setting: render as checkbox
            auto *row = new QHBoxLayout;
            auto *label = new QLabel(itemLabel);
            label->setAccessibleName(itemLabel);
            auto *checkbox = new QCheckBox;
            checkbox->setAccessibleName(itemLabel);
            checkbox->setChecked(kind["Toggle"].toObject()["enabled"].toBool());
            row->addWidget(label);
            row->addStretch();
            row->addWidget(checkbox);

            auto *rowWidget = new QWidget;
            rowWidget->setLayout(row);
            layout->addWidget(rowWidget);

            if (onAction) {
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
        } else if (kind.contains("Link") || kind.contains("Destructive")) {
            // Link or destructive setting: render as clickable button
            auto *btn = new QPushButton(itemLabel);
            btn->setAccessibleName(itemLabel);
            if (kind.contains("Destructive")) {
                btn->setStyleSheet("color: red;");
            }
            layout->addWidget(btn);

            if (onAction) {
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
        } else {
            // Value or unknown kind: render as label
            auto *label = new QLabel(itemLabel);
            layout->addWidget(label);
        }
    }

    return group;
}
