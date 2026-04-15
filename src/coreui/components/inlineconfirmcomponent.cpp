// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "inlineconfirmcomponent.h"
#include "Tokens.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

QWidget *InlineConfirmComponent::render(const QJsonObject &data,
                                        const OnAction &onAction) {
    auto *frame = new QFrame;
    frame->setFrameStyle(QFrame::Box | QFrame::Raised);
    frame->setObjectName(data["id"].toString());
    frame->setAccessibleName(data["warning"].toString());

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(Tokens::Spacing::MD, Tokens::Spacing::MD,
                               Tokens::Spacing::MD, Tokens::Spacing::MD);

    // Warning icon + text
    auto *warningRow = new QHBoxLayout;
    auto *icon = new QLabel(QStringLiteral("\u26A0"));
    warningRow->addWidget(icon);

    auto *warningLabel = new QLabel(data["warning"].toString());
    warningLabel->setWordWrap(true);
    if (data["destructive"].toBool()) {
        warningLabel->setStyleSheet("color: red;");
    }
    warningRow->addWidget(warningLabel, 1);
    layout->addLayout(warningRow);

    // Button row
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();

    // Cancel button
    QString cancelText = data["cancel_text"].toString("Cancel");
    auto *cancelBtn = new QPushButton(cancelText);
    cancelBtn->setFlat(true);
    buttonRow->addWidget(cancelBtn);

    // Confirm button
    QString confirmText = data["confirm_text"].toString("Confirm");
    auto *confirmBtn = new QPushButton(confirmText);
    if (data["destructive"].toBool()) {
        confirmBtn->setStyleSheet("color: white; background-color: #e74c3c; border-radius: 4px; padding: 6px 16px;");
    } else {
        confirmBtn->setStyleSheet("color: white; background-color: #3498db; border-radius: 4px; padding: 6px 16px;");
    }
    buttonRow->addWidget(confirmBtn);
    layout->addLayout(buttonRow);

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) {
            frame->setAccessibleName(a11yLabel);
        }
        auto hint = a11y.value("hint").toString();
        if (!hint.isEmpty()) {
            frame->setAccessibleDescription(hint);
        }
    }

    if (onAction) {
        QString dialogId = data["id"].toString();

        QObject::connect(cancelBtn, &QPushButton::clicked, cancelBtn,
                         [onAction, dialogId]() {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["action_id"] = dialogId + "_cancel";
                             action["ActionPressed"] = inner;
                             onAction(action);
                         });

        QObject::connect(confirmBtn, &QPushButton::clicked, confirmBtn,
                         [onAction, dialogId]() {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["action_id"] = dialogId + "_confirm";
                             action["ActionPressed"] = inner;
                             onAction(action);
                         });
    }

    return frame;
}
