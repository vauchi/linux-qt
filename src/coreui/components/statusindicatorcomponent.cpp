// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusindicatorcomponent.h"
#include <QLabel>
#include <QHBoxLayout>

QWidget *StatusIndicatorComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *indicator = new QLabel;
    QString status = data["status"].toString();
    // TODO: Use proper icons/colors for status states
    indicator->setText(status);

    auto *label = new QLabel(data["label"].toString());
    layout->addWidget(indicator);
    layout->addWidget(label);
    layout->addStretch();

    return container;
}
