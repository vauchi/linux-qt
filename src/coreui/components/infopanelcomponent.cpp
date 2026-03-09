// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "infopanelcomponent.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>

QWidget *InfoPanelComponent::render(const QJsonObject &data) {
    auto *frame = new QFrame;
    frame->setFrameStyle(QFrame::StyledPanel);
    auto *layout = new QVBoxLayout(frame);

    auto *title = new QLabel(data["title"].toString());
    title->setStyleSheet("font-weight: bold;");
    layout->addWidget(title);

    auto *body = new QLabel(data["body"].toString());
    body->setWordWrap(true);
    layout->addWidget(body);

    // TODO: Support info panel variants (info, warning, error)
    return frame;
}
