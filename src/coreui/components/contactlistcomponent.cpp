// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contactlistcomponent.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QJsonArray>

QWidget *ContactListComponent::render(const QJsonObject &data,
                                      const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QString componentId = data["id"].toString();
    bool searchable = data["searchable"].toBool(false);

    // Search input (only when core says the list is searchable)
    if (searchable && onAction) {
        auto *search = new QLineEdit;
        search->setPlaceholderText(QObject::tr("Search contacts..."));
        search->setClearButtonEnabled(true);
        search->setAccessibleName(QStringLiteral("Search contacts"));
        layout->addWidget(search);

        QObject::connect(search, &QLineEdit::textChanged, search,
                         [onAction, componentId](const QString &text) {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["component_id"] = componentId;
                             inner["query"] = text;
                             action["SearchChanged"] = inner;
                             onAction(action);
                         });
    }

    auto *list = new QListWidget;
    list->setObjectName(componentId);
    list->setAccessibleName(QStringLiteral("Contacts"));

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

    layout->addWidget(list);
    return container;
}
