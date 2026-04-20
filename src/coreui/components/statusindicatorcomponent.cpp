// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusindicatorcomponent.h"
#include "../thememanager.h"
#include <QLabel>
#include <QHBoxLayout>

QWidget *StatusIndicatorComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    container->setObjectName(data["id"].toString());
    container->setAccessibleName(data["title"].toString());
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    // Status dot with color
    auto *dot = new QLabel(QStringLiteral("\u25CF"));
    QString status = data["status"].toString();
    if (status == "Success") {
        dot->setStyleSheet(ThemeManager::styleForRole(ThemeRole::StatusSuccess));
    } else if (status == "Failed") {
        dot->setStyleSheet(ThemeManager::styleForRole(ThemeRole::StatusError));
    } else if (status == "Warning") {
        dot->setStyleSheet(ThemeManager::styleForRole(ThemeRole::StatusWarning));
    } else if (status == "InProgress") {
        dot->setStyleSheet(ThemeManager::styleForRole(ThemeRole::StatusInProgress));
    } else {
        dot->setStyleSheet(ThemeManager::styleForRole(ThemeRole::StatusNeutral));
    }

    auto *title = new QLabel(data["title"].toString());
    title->setAccessibleName(data["title"].toString());
    layout->addWidget(dot);
    layout->addWidget(title);

    // Optional detail text
    QString detail = data["detail"].toString();
    if (!detail.isEmpty()) {
        auto *detailLabel = new QLabel(detail);
        detailLabel->setAccessibleName(data["title"].toString() + " detail");
        detailLabel->setStyleSheet(ThemeManager::styleForRole(ThemeRole::SecondaryText));
        layout->addWidget(detailLabel);
    }

    layout->addStretch();

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto label = a11y.value("label").toString();
        if (!label.isEmpty()) {
            container->setAccessibleName(label);
        }
        auto hint = a11y.value("hint").toString();
        if (!hint.isEmpty()) {
            container->setAccessibleDescription(hint);
        }
    }

    return container;
}
