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

namespace {

class RowAction : public QToolButton {
public:
    RowAction(QWidget *content, bool hasMenu) {
        auto *box = new QHBoxLayout(this);
        const int trailing = hasMenu ? style()->pixelMetric(QStyle::PM_MenuButtonIndicator) : 0;
        box->setContentsMargins(8, 8, 8 + trailing, 8);
        box->addWidget(content);
        content->setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    QSize sizeHint() const override { return layout()->totalSizeHint(); }
    QSize minimumSizeHint() const override { return layout()->totalMinimumSize(); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return layout()->totalHeightForWidth(width); }
};

QWidget *rowCopy(const QJsonObject &row) {
    auto *content = new QWidget;
    auto *box = new QHBoxLayout(content);
    box->setContentsMargins(0, 0, 0, 0);
    const auto pictogram = vauchi::contentPictogram(row.value("icon_token").toString());
    if (!pictogram.isNull()) {
        auto *icon = new QLabel;
        icon->setPixmap(pictogram.pixmap(vauchi::kPictogramSize, vauchi::kPictogramSize));
        box->addWidget(icon);
    }
    auto *copy = new QVBoxLayout;
    copy->setSpacing(2);
    for (const char *key : {"title", "subtitle"}) {
        const auto text = row.value(QLatin1String(key)).toString();
        if (text.isEmpty()) continue;
        auto *label = new QLabel(text);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignLeading | Qt::AlignVCenter);
        if (QLatin1String(key) != QLatin1String("title")) label->setProperty("tone", "muted");
        copy->addWidget(label);
    }
    box->addLayout(copy, 1);
    return content;
}

}

QWidget *PresentationSurface::renderList(const QJsonObject &payload) {
    auto *group =
        new QGroupBox(payload.value(QStringLiteral("label")).toString());
    auto *layout = new QVBoxLayout(group);
    for (const auto &rowValue :
         payload.value(QStringLiteral("rows")).toArray()) {
        const QJsonObject row = rowValue.toObject();
        const QJsonObject activation =
            row.value(QStringLiteral("activation")).toObject();
        const QJsonArray secondary = row.value(QStringLiteral("secondary_actions")).toArray();
        auto *copy = rowCopy(row);
        QToolButton *button = nullptr;
        if (!activation.isEmpty() || !secondary.isEmpty()) {
            button = new RowAction(copy, !secondary.isEmpty());
            button->setEnabled(row.value(QStringLiteral("enabled")).toBool(true));
            button->setMinimumHeight(m_minimumTargetSize);
            button->setStyleSheet(targetSizeStyleSheet());
            applyAccessibility(button, row.value(QStringLiteral("accessibility")).toObject());
        }
        if (!activation.isEmpty()) {
            button->setObjectName(
                activation.value(QStringLiteral("interaction_id")).toString());
            connect(button, &QToolButton::clicked, this,
                    [this, activation]() { activate(activation); });
        }
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
        auto *rowWidget = new QWidget;
        auto *rowContents = new QVBoxLayout(rowWidget);
        rowContents->setContentsMargins(0, 0, 0, 0);
        auto *line = new QWidget;
        auto *rowLayout = new QHBoxLayout(line);
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
        auto *textColumn = new QWidget;
        auto *textLayout = new QVBoxLayout(textColumn);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(0);
        textLayout->addWidget(button ? static_cast<QWidget *>(button) : copy);
        const auto detail = row.value(QStringLiteral("detail")).toString();
        if (!detail.isEmpty()) {
            // Native buttons hide descendants, so detail stays separately readable (#1109).
            auto *label = new QLabel(detail);
            label->setTextFormat(Qt::PlainText);
            label->setWordWrap(true);
            label->setAlignment(Qt::AlignLeading | Qt::AlignVCenter);
            label->setProperty("tone", "muted");
            if (button) label->setContentsMargins(8, 0, 8, 0);
            textLayout->addWidget(label);
        }
        rowLayout->addWidget(textColumn, 1);
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
        const auto controls = row.value(QStringLiteral("controls")).toArray();
        const bool inlineToggle = controls.size() == 1 && controls.first().toObject().contains("Toggle");
        if (inlineToggle) rowLayout->addWidget(renderNode(controls.first()), 0, Qt::AlignVCenter);
        rowContents->addWidget(line);
        if (!inlineToggle) {
            for (const auto &control : controls) rowContents->addWidget(renderNode(control));
        }
        layout->addWidget(rowWidget);
    }
    applyAccessibility(
        group, payload.value(QStringLiteral("accessibility")).toObject());
    return group;
}
