// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "confirmationdialogcomponent.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

QWidget *ConfirmationDialogComponent::render(const QJsonObject &data,
                                             const OnAction &onAction) {
    auto *container = new QWidget;
    container->setObjectName(data["id"].toString());
    container->setAccessibleName(data["title"].toString());
    auto *layout = new QVBoxLayout(container);

    auto *message = new QLabel(data["message"].toString());
    message->setWordWrap(true);
    layout->addWidget(message);

    auto *buttonLayout = new QHBoxLayout;
    auto *confirmBtn = new QPushButton(data["confirm_text"].toString("Confirm"));
    auto *cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(confirmBtn);
    layout->addLayout(buttonLayout);

    if (onAction) {
        QString dialogId = data["id"].toString();

        QObject::connect(confirmBtn, &QPushButton::clicked, confirmBtn,
                         [onAction, dialogId]() {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["action_id"] = dialogId + "_confirm";
                             action["ActionPressed"] = inner;
                             onAction(action);
                         });

        QObject::connect(cancelBtn, &QPushButton::clicked, cancelBtn,
                         [onAction, dialogId]() {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["action_id"] = dialogId + "_cancel";
                             action["ActionPressed"] = inner;
                             onAction(action);
                         });
    }

    return container;
}
