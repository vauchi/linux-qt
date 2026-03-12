// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pininputcomponent.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>

QWidget *PinInputComponent::render(const QJsonObject &data,
                                   const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *label = new QLabel(data["label"].toString());
    auto *input = new QLineEdit;
    input->setEchoMode(QLineEdit::Password);
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

    return container;
}
