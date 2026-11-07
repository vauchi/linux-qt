// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationsurface.h"
#include "navigationicons.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

QWidget *PresentationSurface::renderList(const QJsonObject &payload) {
    auto *group =
        new QGroupBox(payload.value(QStringLiteral("label")).toString());
    auto *layout = new QVBoxLayout(group);
    for (const auto &rowValue :
         payload.value(QStringLiteral("rows")).toArray()) {
        const QJsonObject row = rowValue.toObject();
        auto *button = new QToolButton;
        QString text = row.value(QStringLiteral("title")).toString();
        const QString subtitle =
            row.value(QStringLiteral("subtitle")).toString();
        if (!subtitle.isEmpty()) {
            text += QStringLiteral("\n") + subtitle;
        }
        button->setText(text);
        const QIcon pictogram = vauchi::contentPictogram(
            row.value(QStringLiteral("icon_token")).toString());
        if (pictogram.isNull()) {
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        } else {
            button->setIcon(pictogram);
            button->setIconSize(
                QSize(vauchi::kPictogramSize, vauchi::kPictogramSize));
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        }
        button->setEnabled(row.value(QStringLiteral("enabled")).toBool(true));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        button->setMinimumHeight(m_minimumTargetSize);
        button->setStyleSheet(targetSizeStyleSheet());
        const QJsonObject activation =
            row.value(QStringLiteral("activation")).toObject();
        if (!activation.isEmpty()) {
            button->setObjectName(
                activation.value(QStringLiteral("interaction_id")).toString());
            connect(button, &QToolButton::clicked, this,
                    [this, activation]() { activate(activation); });
        }
        const QJsonArray secondary =
            row.value(QStringLiteral("secondary_actions")).toArray();
        if (!secondary.isEmpty()) {
            auto *menu = new QMenu(button);
            for (const auto &actionValue : secondary) {
                const QJsonObject action = actionValue.toObject();
                // QAction exposes no accessible-name API, so its Core label
                // would be dropped; a widget action carries one.
                auto *item = new QToolButton(menu);
                item->setText(action.value(QStringLiteral("label")).toString());
                item->setToolButtonStyle(Qt::ToolButtonTextOnly);
                item->setAutoRaise(true);
                item->setSizePolicy(QSizePolicy::Expanding,
                                    QSizePolicy::Preferred);
                item->setEnabled(
                    action.value(QStringLiteral("enabled")).toBool(true));
                item->setAccessibleName(
                    action.value(QStringLiteral("accessibility_label"))
                        .toString());
                auto *menuAction = new QWidgetAction(menu);
                // A widget action paints its default widget, so the action's
                // own text is free to be the accessible name Qt reads for the
                // menu item.
                menuAction->setText(
                    action.value(QStringLiteral("accessibility_label"))
                        .toString());
                menuAction->setDefaultWidget(item);
                menu->addAction(menuAction);
                connect(item, &QToolButton::clicked, this,
                        [this, action, menu]() {
                            menu->hide();
                            activate(action);
                        });
            }
            button->setMenu(menu);
            button->setPopupMode(QToolButton::MenuButtonPopup);
        }
        applyAccessibility(
            button, row.value(QStringLiteral("accessibility")).toObject());

        auto *rowWidget = new QWidget;
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        const QJsonArray avatar =
            row.value(QStringLiteral("image_data")).toArray();
        const QString initials =
            row.value(QStringLiteral("fallback_text")).toString();
        if (!avatar.isEmpty()) {
            QByteArray bytes;
            bytes.reserve(avatar.size());
            for (const auto &byte : avatar) {
                bytes.append(static_cast<char>(byte.toInt()));
            }
            QPixmap pixmap;
            if (pixmap.loadFromData(bytes)) {
                auto *image = new QLabel;
                image->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
                rowLayout->addWidget(image);
            }
        } else if (!initials.isEmpty()) {
            rowLayout->addWidget(new QLabel(initials));
        }
        rowLayout->addWidget(button, 1);
        const QString detail = row.value(QStringLiteral("detail")).toString();
        if (!detail.isEmpty()) {
            auto *detailLabel = new QLabel(detail);
            detailLabel->setProperty("tone", "muted");
            rowLayout->addWidget(detailLabel);
        }
        // Explains this one item (vauchi/private#479); Core names it
        // "About <item>" so a screen reader says what the icon is for.
        const QJsonObject info = row.value(QStringLiteral("info")).toObject();
        if (!info.isEmpty()) {
            auto *infoButton = new QToolButton;
            infoButton->setObjectName(
                info.value(QStringLiteral("interaction_id")).toString());
            infoButton->setIcon(QIcon::fromTheme(
                QStringLiteral("help-about"),
                infoButton->style()->standardIcon(
                    QStyle::SP_MessageBoxInformation)));
            infoButton->setAutoRaise(true);
            infoButton->setEnabled(
                info.value(QStringLiteral("enabled")).toBool(true));
            infoButton->setMinimumSize(m_minimumTargetSize, m_minimumTargetSize);
            infoButton->setToolTip(
                info.value(QStringLiteral("accessibility_label")).toString());
            infoButton->setAccessibleName(
                info.value(QStringLiteral("accessibility_label")).toString());
            connect(infoButton, &QToolButton::clicked, this,
                    [this, info]() { activate(info); });
            rowLayout->addWidget(infoButton);
        }
        // A row without an activation has no button to carry its name, and
        // the avatar has to sit inside whatever does.
        if (activation.isEmpty()) {
            applyAccessibility(
                rowWidget, row.value(QStringLiteral("accessibility")).toObject());
        }
        layout->addWidget(rowWidget);
        for (const auto &control :
             row.value(QStringLiteral("controls")).toArray()) {
            layout->addWidget(renderNode(control));
        }
    }
    applyAccessibility(
        group, payload.value(QStringLiteral("accessibility")).toObject());
    return group;
}
