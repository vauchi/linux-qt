// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sectionedactionlistcomponent.h"
#include "../thememanager.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

/// Renders one item row inside a section: tappable QPushButton showing
/// the label, with an optional detail label aligned to the right and
/// an optional icon prefix. Mirrors iOS SectionedActionRowView shape.
QWidget *renderItem(const QJsonObject &item,
                    const QString &componentId,
                    const OnAction &onAction) {
    auto *row = new QWidget;
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    const QString id    = item[QLatin1String("id")].toString();
    const QString label = item[QLatin1String("label")].toString();
    const QString icon  = item[QLatin1String("icon")].toString();
    const QString detail = item[QLatin1String("detail")].toString();

    auto *btn = new QPushButton(label);
    btn->setObjectName(componentId + QLatin1Char('.') + id);
    if (!icon.isEmpty()) {
        btn->setIcon(QIcon::fromTheme(icon));
    }

    // a11y: prefer a11y.label / a11y.hint over the visible text.
    QString accLabel = label;
    QString accHint;
    if (item.contains(QLatin1String("a11y")) && item[QLatin1String("a11y")].isObject()) {
        auto a11y = item[QLatin1String("a11y")].toObject();
        auto override_ = a11y.value(QLatin1String("label")).toString();
        if (!override_.isEmpty()) accLabel = override_;
        accHint = a11y.value(QLatin1String("hint")).toString();
    }
    btn->setAccessibleName(accLabel);
    if (!accHint.isEmpty()) btn->setAccessibleDescription(accHint);

    if (onAction) {
        QObject::connect(btn, &QPushButton::clicked, btn,
                         [onAction, componentId, id]() {
                             QJsonObject action;
                             QJsonObject inner;
                             inner[QLatin1String("component_id")] = componentId;
                             inner[QLatin1String("item_id")] = id;
                             action[QLatin1String("ListItemSelected")] = inner;
                             onAction(action);
                         });
    }

    rowLayout->addWidget(btn, 1);
    if (!detail.isEmpty()) {
        auto *detailLabel = new QLabel(detail);
        detailLabel->setStyleSheet(ThemeManager::styleForRole(ThemeRole::SecondaryText));
        detailLabel->setAccessibleName(accLabel + QStringLiteral(" detail"));
        rowLayout->addWidget(detailLabel);
    }

    return row;
}

}  // namespace

QWidget *SectionedActionListComponent::render(const QJsonObject &data,
                                              const OnAction &onAction) {
    auto *container = new QWidget;
    container->setObjectName(data[QLatin1String("id")].toString());
    container->setAccessibleName(QStringLiteral("Menu"));

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    const QString componentId = data[QLatin1String("id")].toString();
    QJsonArray sections = data[QLatin1String("sections")].toArray();

    for (const auto &sectionVal : sections) {
        QJsonObject section = sectionVal.toObject();
        const QString sectionLabel = section[QLatin1String("label")].toString();

        // Section header — emphasized QLabel matching native Qt section style.
        auto *header = new QLabel(sectionLabel);
        header->setStyleSheet(ThemeManager::styleForRole(ThemeRole::SecondaryText));
        header->setAccessibleName(sectionLabel);
        layout->addWidget(header);

        QJsonArray items = section[QLatin1String("items")].toArray();
        for (const auto &itemVal : items) {
            QJsonObject item = itemVal.toObject();
            QWidget *row = renderItem(item, componentId, onAction);
            layout->addWidget(row);
        }
    }

    return container;
}
