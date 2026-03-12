// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "confirmationdialogcomponent.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

QWidget *ConfirmationDialogComponent::render(const QJsonObject &data) {
    auto *container = new QWidget;
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

    // TODO: Connect buttons to workflow actions
    return container;
}
