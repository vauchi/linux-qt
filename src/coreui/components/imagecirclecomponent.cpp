// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagecirclecomponent.h"
#include <QByteArray>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
// Convert canonical [r, g, b] u8 array to a "color: #rrggbb;" stylesheet
// fragment, or empty string if absent / malformed.
QString bgColorStyle(const QJsonObject &data) {
    if (!data.contains("bg_color") || !data["bg_color"].isArray()) return {};
    auto arr = data["bg_color"].toArray();
    if (arr.size() != 3) return {};
    return QStringLiteral("background-color: rgb(%1, %2, %3);")
        .arg(arr[0].toInt())
        .arg(arr[1].toInt())
        .arg(arr[2].toInt());
}
} // namespace

QWidget *ImageCircleComponent::render(const QJsonObject &data,
                                      const OnAction &onAction) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    QString componentId = data["id"].toString();
    container->setObjectName(componentId);

    constexpr int kAvatarPx = 96;
    auto *avatarHost = new QWidget;
    avatarHost->setFixedSize(kAvatarPx, kAvatarPx);
    auto *avatarLayout = new QGridLayout(avatarHost);
    avatarLayout->setContentsMargins(0, 0, 0, 0);
    auto *avatar = new QLabel;
    avatar->setFixedSize(kAvatarPx, kAvatarPx);
    avatar->setAlignment(Qt::AlignCenter);

    bool hasImage = false;
    if (data.contains("image_data") && data["image_data"].isArray()) {
        // image_data is a serde-default Vec<u8> — JSON array of bytes.
        QByteArray bytes;
        auto arr = data["image_data"].toArray();
        bytes.reserve(arr.size());
        for (const auto &b : arr) {
            bytes.append(static_cast<char>(b.toInt() & 0xff));
        }
        QPixmap pix;
        if (!bytes.isEmpty() && pix.loadFromData(bytes)) {
            avatar->setPixmap(pix.scaled(kAvatarPx, kAvatarPx,
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
            hasImage = true;
        }
    }

    if (!hasImage) {
        avatar->setText(data["initials"].toString());
        QString bgStyle = bgColorStyle(data);
        avatar->setStyleSheet(
            QStringLiteral("border-radius: %1px; color: white; %2")
                .arg(kAvatarPx / 2)
                .arg(bgStyle));
    }

    QString a11yLabel;
    if (data.contains("a11y") && data["a11y"].isObject()) {
        a11yLabel = data["a11y"].toObject().value("label").toString();
    }
    QString a11yHint;
    if (data.contains("a11y") && data["a11y"].isObject()) {
        a11yHint = data["a11y"].toObject().value("hint").toString();
    }
    if (!a11yLabel.isEmpty()) avatar->setAccessibleName(a11yLabel);
    if (!a11yHint.isEmpty()) avatar->setAccessibleDescription(a11yHint);
    avatarLayout->addWidget(avatar, 0, 0);

    const QString editActionId = data["edit_action_id"].toString();
    const bool editable = data["editable"].toBool() && !editActionId.isEmpty();
    if (editable) {
        auto *editButton = new QPushButton(QStringLiteral("\u270e"), avatarHost);
        editButton->setObjectName(QStringLiteral("image-edit-affordance"));
        editButton->setFixedSize(32, 32);
        editButton->setFlat(true);
        editButton->setStyleSheet(QStringLiteral(
            "QPushButton { border-radius: 16px; font-size: 18px; }"));
        if (!a11yLabel.isEmpty()) editButton->setAccessibleName(a11yLabel);
        if (!a11yHint.isEmpty()) editButton->setAccessibleDescription(a11yHint);
        if (onAction) {
            QObject::connect(editButton, &QPushButton::clicked, editButton,
                             [onAction, editActionId]() {
                                 QJsonObject action;
                                 QJsonObject inner;
                                 inner["action_id"] = editActionId;
                                 action["ActionPressed"] = inner;
                                 onAction(action);
                             });
        }
        avatarLayout->addWidget(editButton, 0, 0, Qt::AlignRight | Qt::AlignBottom);
    }

    layout->addWidget(avatarHost, 0, Qt::AlignCenter);
    return container;
}
