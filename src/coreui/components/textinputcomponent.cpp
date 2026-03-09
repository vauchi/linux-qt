// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textinputcomponent.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>

QWidget *TextInputComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *label = new QLabel(data["label"].toString());
    auto *input = new QLineEdit;
    input->setPlaceholderText(data["placeholder"].toString());
    input->setText(data["value"].toString());

    layout->addWidget(label);
    layout->addWidget(input);

    // TODO: Emit value changes back to workflow
    return container;
}
