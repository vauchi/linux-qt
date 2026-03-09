// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textinputcomponent.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>

QWidget *TextInputComponent::render(const QJsonObject &data,
                                    const OnComponentChanged &onChange) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *label = new QLabel(data["label"].toString());
    auto *input = new QLineEdit;
    input->setPlaceholderText(data["placeholder"].toString());
    input->setText(data["value"].toString());

    if (data.contains("max_length") && !data["max_length"].isNull()) {
        input->setMaxLength(data["max_length"].toInt());
    }

    if (onChange) {
        QString componentId = data["id"].toString();
        QObject::connect(input, &QLineEdit::textChanged, input,
                         [onChange, componentId](const QString &text) {
                             onChange(componentId, text);
                         });
    }

    layout->addWidget(label);
    layout->addWidget(input);

    return container;
}
