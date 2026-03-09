// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cardpreviewcomponent.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>

QWidget *CardPreviewComponent::render(const QJsonObject &data) {
    auto *frame = new QFrame;
    frame->setFrameStyle(QFrame::Box | QFrame::Raised);
    auto *layout = new QVBoxLayout(frame);

    auto *name = new QLabel(data["display_name"].toString());
    name->setStyleSheet("font-size: 18px; font-weight: bold;");
    layout->addWidget(name);

    // TODO: Render contact fields and groups
    return frame;
}
