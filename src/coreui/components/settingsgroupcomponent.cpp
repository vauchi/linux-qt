// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsgroupcomponent.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonArray>

QWidget *SettingsGroupComponent::render(const QJsonObject &data) {
    auto *group = new QGroupBox(data["title"].toString());
    auto *layout = new QVBoxLayout(group);

    QJsonArray settings = data["settings"].toArray();
    for (const auto &setting : settings) {
        QJsonObject settingObj = setting.toObject();
        auto *label = new QLabel(
            settingObj["label"].toString() + ": " + settingObj["value"].toString()
        );
        layout->addWidget(label);
    }

    // TODO: Support setting editing
    return group;
}
