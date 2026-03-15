// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textinputcomponent.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>

QWidget *TextInputComponent::render(const QJsonObject &data,
                                    const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    container->setObjectName(data["id"].toString());

    auto *label = new QLabel(data["label"].toString());
    auto *input = new QLineEdit;
    input->setPlaceholderText(data["placeholder"].toString());
    input->setText(data["value"].toString());
    input->setAccessibleName(data["label"].toString());
    input->setObjectName(data["id"].toString() + "_input");

    if (data.contains("max_length") && !data["max_length"].isNull()) {
        input->setMaxLength(data["max_length"].toInt());
    }

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
