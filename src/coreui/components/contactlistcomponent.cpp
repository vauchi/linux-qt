// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contactlistcomponent.h"
#include <QListWidget>
#include <QJsonArray>

QWidget *ContactListComponent::render(const QJsonObject &data,
                                      const OnAction &onAction) {
    auto *list = new QListWidget;

    QString componentId = data["id"].toString();

    QJsonArray contacts = data["contacts"].toArray();
    for (const auto &contact : contacts) {
        QJsonObject contactObj = contact.toObject();
        auto *item = new QListWidgetItem(contactObj["name"].toString());
        item->setData(Qt::UserRole, contactObj["id"].toString());
        list->addItem(item);
    }

    if (onAction) {
        QObject::connect(list, &QListWidget::currentItemChanged, list,
                         [onAction, componentId](QListWidgetItem *current, QListWidgetItem * /*previous*/) {
                             if (!current) return;
                             QString itemId = current->data(Qt::UserRole).toString();
                             QJsonObject action;
                             QJsonObject inner;
                             inner["component_id"] = componentId;
                             inner["item_id"] = itemId;
                             action["ListItemSelected"] = inner;
                             onAction(action);
                         });
    }

    return list;
}
