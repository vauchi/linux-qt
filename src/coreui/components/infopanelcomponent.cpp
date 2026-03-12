// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "infopanelcomponent.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QJsonArray>

QWidget *InfoPanelComponent::render(const QJsonObject &data) {
    auto *frame = new QFrame;
    frame->setFrameStyle(QFrame::StyledPanel);
    auto *layout = new QVBoxLayout(frame);

    // Optional icon + title row
    QString icon = data["icon"].toString();
    auto *title = new QLabel(
        (icon.isEmpty() ? QString() : icon + " ") + data["title"].toString()
    );
    title->setStyleSheet("font-weight: bold;");
    layout->addWidget(title);

    QJsonArray items = data["items"].toArray();
    for (const auto &item : items) {
        QJsonObject itemObj = item.toObject();
        auto *row = new QHBoxLayout;
        QString itemIcon = itemObj["icon"].toString();
        if (!itemIcon.isEmpty()) {
            auto *iconLabel = new QLabel(itemIcon);
            row->addWidget(iconLabel);
        }
        auto *itemTitle = new QLabel(itemObj["title"].toString() + ":");
        itemTitle->setStyleSheet("font-weight: bold;");
        auto *detail = new QLabel(itemObj["detail"].toString());
        detail->setWordWrap(true);
        row->addWidget(itemTitle);
        row->addWidget(detail, 1);
        layout->addLayout(row);
    }

    return frame;
}
