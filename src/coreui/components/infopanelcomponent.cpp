// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "infopanelcomponent.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>
#include <QJsonArray>

QWidget *InfoPanelComponent::render(const QJsonObject &data) {
    auto *frame = new QFrame;
    frame->setFrameStyle(QFrame::StyledPanel);
    auto *layout = new QVBoxLayout(frame);

    auto *title = new QLabel(data["title"].toString());
    title->setStyleSheet("font-weight: bold;");
    layout->addWidget(title);

    QJsonArray items = data["items"].toArray();
    for (const auto &item : items) {
        QJsonObject itemObj = item.toObject();
        auto *row = new QLabel(
            itemObj["icon"].toString() + " " +
            itemObj["title"].toString() + ": " +
            itemObj["detail"].toString()
        );
        row->setWordWrap(true);
        layout->addWidget(row);
    }

    // TODO: Support info panel variants (info, warning, error)
    return frame;
}
