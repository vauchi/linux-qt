// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigationsidebar.h"
#include "navigationicons.h"

#include <QJsonArray>
#include <QToolButton>
#include <QVBoxLayout>

namespace vauchi {

QList<NavigationRow> navigationRows(const QJsonObject &navigation) {
    const QJsonArray items =
        navigation.value(QStringLiteral("items")).toArray();
    QList<NavigationRow> rows;
    rows.reserve(items.size());
    for (const auto &itemValue : items) {
        const QJsonObject item = itemValue.toObject();
        rows.append(NavigationRow{
            item.value(QStringLiteral("interaction_id")).toString(),
            item.value(QStringLiteral("label")).toString(),
            item.value(QStringLiteral("accessibility_label")).toString(),
            item.value(QStringLiteral("icon_token")).toString(),
            item.value(QStringLiteral("selected")).toBool(false),
            item.value(QStringLiteral("badge_count")).toInt(0),
        });
    }
    return rows;
}

int selectedNavigationRow(const QList<NavigationRow> &rows) {
    for (int i = 0; i < rows.size(); ++i) {
        if (rows.at(i).selected) {
            return i;
        }
    }
    return -1;
}

} // namespace vauchi

NavigationSidebar::NavigationSidebar(const QJsonObject &navigation,
                                     QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    for (const vauchi::NavigationRow &row :
         vauchi::navigationRows(navigation)) {
        auto *button = new QToolButton(this);
        // Shared, not unique: the id selector in ThemeManager's stylesheet
        // (`QToolButton#navigation-sidebar-row:checked`) is meant to match
        // every row, and Qt applies an id-selector rule to every widget
        // carrying that objectName.
        button->setObjectName(QStringLiteral("navigation-sidebar-row"));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIcon(vauchi::navigationIcon(row.iconToken));
        button->setText(row.badgeCount > 0
                            ? QStringLiteral("%1 (%2)")
                                  .arg(row.label)
                                  .arg(row.badgeCount)
                            : row.label);
        button->setAccessibleName(row.accessibilityLabel);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setChecked(row.selected);
        // QToolButton defaults to Tab-only focus in some styles; the row
        // needs the same click-and-keyboard focusability as every other
        // interactive control ThemeManager's `:focus` rule already covers.
        button->setFocusPolicy(Qt::StrongFocus);
        const QString interactionId = row.interactionId;
        connect(button, &QToolButton::clicked, this,
                [this, interactionId]() { emit interactionReady(interactionId); });
        layout->addWidget(button);
    }
    layout->addStretch(1);
}
