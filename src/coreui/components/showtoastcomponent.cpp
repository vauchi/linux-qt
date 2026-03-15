// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "showtoastcomponent.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

QWidget *ShowToastComponent::render(const QJsonObject &data,
                                    const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    container->setObjectName(data["id"].toString());
    container->setAccessibleName(data["message"].toString());
    container->setStyleSheet("background-color: #333; color: white; border-radius: 4px; padding: 8px;");

    // Info icon
    auto *icon = new QLabel(QStringLiteral("\u2139"));
    layout->addWidget(icon);

    // Message
    auto *msg = new QLabel(data["message"].toString());
    msg->setWordWrap(true);
    layout->addWidget(msg, 1);

    // Optional Undo button
    QString undoActionId = data["undo_action_id"].toString();
    if (!undoActionId.isEmpty() && onAction) {
        auto *undoBtn = new QPushButton(QStringLiteral("Undo"));
        undoBtn->setFlat(true);
        undoBtn->setStyleSheet("color: #4fc3f7;");

        QString toastId = data["id"].toString();
        QObject::connect(undoBtn, &QPushButton::clicked, undoBtn,
                         [onAction, toastId, undoActionId]() {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["action_id"] = toastId + "_" + undoActionId;
                             action["UndoPressed"] = inner;
                             onAction(action);
                         });
        layout->addWidget(undoBtn);
    }

    return container;
}
