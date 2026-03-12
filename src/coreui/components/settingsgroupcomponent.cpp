// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsgroupcomponent.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonArray>

QWidget *SettingsGroupComponent::render(const QJsonObject &data) {
    auto *group = new QGroupBox(data["label"].toString());
    auto *layout = new QVBoxLayout(group);

    QJsonArray items = data["items"].toArray();
    for (const auto &item : items) {
        QJsonObject itemObj = item.toObject();
        // TODO: Parse kind object (Toggle/Value/Link/Destructive) for richer rendering
        auto *label = new QLabel(itemObj["label"].toString());
        layout->addWidget(label);
    }

    // TODO: Support setting editing
    return group;
}
