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
    frame->setObjectName(data["id"].toString());
    frame->setAccessibleName(data["title"].toString());
    auto *layout = new QVBoxLayout(frame);

    // Optional icon + title row
    QString icon = data["icon"].toString();
    auto *title = new QLabel(
        (icon.isEmpty() ? QString() : icon + " ") + data["title"].toString()
    );
    title->setStyleSheet("font-weight: bold;");
    title->setAccessibleName(data["title"].toString());
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
        itemTitle->setAccessibleName(itemObj["title"].toString());
        auto *detail = new QLabel(itemObj["detail"].toString());
        detail->setWordWrap(true);
        detail->setAccessibleName(itemObj["title"].toString() + " detail");
        row->addWidget(itemTitle);
        row->addWidget(detail, 1);
        layout->addLayout(row);
    }

    if (data.contains("a11y") && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto a11yLabel = a11y.value("label").toString();
        if (!a11yLabel.isEmpty()) {
            frame->setAccessibleName(a11yLabel);
        }
        auto hint = a11y.value("hint").toString();
        if (!hint.isEmpty()) {
            frame->setAccessibleDescription(hint);
        }
    }

    return frame;
}
