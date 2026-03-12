// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contactlistcomponent.h"
#include <QListWidget>
#include <QJsonArray>

QWidget *ContactListComponent::render(const QJsonObject &data) {
    auto *list = new QListWidget;

    QJsonArray contacts = data["contacts"].toArray();
    for (const auto &contact : contacts) {
        QJsonObject contactObj = contact.toObject();
        list->addItem(contactObj["name"].toString());
    }

    // TODO: Support contact selection and context menu
    return list;
}
