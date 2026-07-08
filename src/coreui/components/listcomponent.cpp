// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "listcomponent.h"
#include "../../i18n.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QJsonArray>

QWidget *ListComponent::render(const QJsonObject &data,
                                      const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QString componentId = data["id"].toString();
    bool searchable = data["searchable"].toBool(false);

    // Search input (only when core says the list is searchable)
    // TODO(HUMBLE): W — list uses "Search contacts" placeholder/accessible name instead of core-supplied generic labels (see _private/docs/problems/2026-07-06-desktop-tui-web-domain-shell-violations)
    if (searchable && onAction) {
        auto *search = new QLineEdit;
        search->setPlaceholderText(tr_vauchi("search.contacts", "Search contacts"));
        search->setClearButtonEnabled(true);
        search->setAccessibleName(tr_vauchi("a11y.search_contacts", "Search contacts"));
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
    // TODO(HUMBLE): W — list hardcodes "Contacts" accessible name instead of core-supplied label (see _private/docs/problems/2026-07-06-desktop-tui-web-domain-shell-violations)
    list->setAccessibleName(tr_vauchi("contacts.title", "Contacts"));

    QJsonArray items = data["items"].toArray();
    for (const auto &entry : items) {
        QJsonObject obj = entry.toObject();
        auto *widgetItem = new QListWidgetItem(obj["name"].toString());
        widgetItem->setData(Qt::UserRole, obj["id"].toString());
        list->addItem(widgetItem);
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
