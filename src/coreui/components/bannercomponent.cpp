// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bannercomponent.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

QWidget *BannerComponent::render(const QJsonObject &data,
                                  const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    // Banner has no "id" field — derive object name from action_id for uniqueness
    container->setObjectName(QStringLiteral("banner_") + data["action_id"].toString());
    container->setAccessibleName(data["text"].toString());
    container->setStyleSheet("background-color: #e3f2fd; border-radius: 4px; padding: 8px;");

    auto *label = new QLabel(data["text"].toString());
    label->setWordWrap(true);
    layout->addWidget(label, 1);

    QString actionLabel = data["action_label"].toString();
    if (!actionLabel.isEmpty() && onAction) {
        auto *btn = new QPushButton(actionLabel);
        QString actionId = data["action_id"].toString();
        QObject::connect(btn, &QPushButton::clicked, btn,
                         [onAction, actionId]() {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["action_id"] = actionId;
                             action["ActionPressed"] = inner;
                             onAction(action);
                         });
        layout->addWidget(btn);
    }

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) {
            container->setAccessibleName(a11yLabel);
        }
        auto hint = a11y.value("hint").toString();
        if (!hint.isEmpty()) {
            container->setAccessibleDescription(hint);
        }
    }

    return container;
}
