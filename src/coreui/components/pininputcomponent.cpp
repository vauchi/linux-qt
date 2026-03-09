// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pininputcomponent.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>

QWidget *PinInputComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *label = new QLabel(data["label"].toString());
    auto *input = new QLineEdit;
    input->setEchoMode(QLineEdit::Password);
    input->setMaxLength(data["length"].toInt(6));
    input->setAlignment(Qt::AlignCenter);

    layout->addWidget(label);
    layout->addWidget(input);

    // TODO: Emit PIN value changes back to workflow
    return container;
}
