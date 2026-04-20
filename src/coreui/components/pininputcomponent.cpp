// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pininputcomponent.h"
#include "../thememanager.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>

QWidget *PinInputComponent::render(const QJsonObject &data,
                                   const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    container->setObjectName(data["id"].toString());

    auto *label = new QLabel(data["label"].toString());
    label->setAccessibleName(data["label"].toString());
    auto *input = new QLineEdit;
    input->setEchoMode(QLineEdit::Password);
    input->setAccessibleName(data["label"].toString());
    input->setObjectName(data["id"].toString() + "_input");
    input->setMaxLength(data["length"].toInt(6));
    input->setAlignment(Qt::AlignCenter);

    if (onAction) {
        QString componentId = data["id"].toString();
        QObject::connect(input, &QLineEdit::textChanged, input,
                         [onAction, componentId](const QString &text) {
                             QJsonObject action;
                             QJsonObject inner;
                             inner["component_id"] = componentId;
                             inner["value"] = text;
                             action["TextChanged"] = inner;
                             onAction(action);
                         });
    }

    layout->addWidget(label);
    layout->addWidget(input);

    // Validation error (injected by core via AppEngine::resolve_validation_error)
    QString error = data["validation_error"].toString();
    if (!error.isEmpty()) {
        auto *errLabel = new QLabel(error);
        errLabel->setStyleSheet(ThemeManager::styleForRole(ThemeRole::DestructiveText) +
                                QStringLiteral(" font-size: 12px;"));
        errLabel->setAccessibleName(error);
        errLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(errLabel);
    }

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) {
            input->setAccessibleName(a11yLabel);
        }
        auto hint = a11y.value("hint").toString();
        if (!hint.isEmpty()) {
            input->setAccessibleDescription(hint);
        }
    }

    return container;
}
