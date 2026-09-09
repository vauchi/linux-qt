// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationsurface.h"

#include <QByteArray>
#include <QJsonArray>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

QWidget *PresentationSurface::renderStatus(const QJsonObject &payload) {
    const QJsonObject activation =
        payload.value(QStringLiteral("activation")).toObject();
    QString text = payload.value(QStringLiteral("title")).toString();
    const QString detail = payload.value(QStringLiteral("detail")).toString();
    const QString badge = payload.value(QStringLiteral("badge")).toString();
    if (!detail.isEmpty()) {
        text += QStringLiteral("\n") + detail;
    }
    if (!badge.isEmpty()) {
        text += QStringLiteral(" · ") + badge;
    }
    QWidget *widget = nullptr;
    if (activation.isEmpty()) {
        auto *label = new QLabel(text);
        label->setWordWrap(true);
        widget = label;
    } else {
        auto *button = new QPushButton(text);
        button->setObjectName(
            activation.value(QStringLiteral("interaction_id")).toString());
        connect(button, &QPushButton::clicked, this,
                [this, activation]() { activate(activation); });
        widget = button;
    }
    widget->setProperty(
        "tone", payload.value(QStringLiteral("tone")).toString());
    applyAccessibility(
        widget, payload.value(QStringLiteral("accessibility")).toObject());
    return widget;
}

QWidget *
PresentationSurface::renderConfirmation(const QJsonObject &payload) {
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    auto *warning =
        new QLabel(payload.value(QStringLiteral("warning")).toString());
    warning->setWordWrap(true);
    layout->addWidget(warning);
    layout->addWidget(
        actionButton(payload.value(QStringLiteral("confirm")).toObject()));
    layout->addWidget(
        actionButton(payload.value(QStringLiteral("cancel")).toObject()));
    applyAccessibility(
        container, payload.value(QStringLiteral("accessibility")).toObject());
    return container;
}

namespace {

/// Side of a standalone avatar. Core names no size for `Image`, so the
/// shell picks one; large enough to read two initials in.
constexpr int kAvatarSide = 96;

/// Core's `shape` was read by nobody here, so an avatar and a diagram were
/// drawn identically. A circle is half the side; a natural image keeps its
/// corners, because rounding them away loses what distinguishes it.
QString avatarStyleSheet(bool circular) {
    return QStringLiteral(
               "QLabel { border-radius: %1px;"
               " background-color: palette(midlight);"
               " color: palette(text);"
               " font-size: 28px; font-weight: bold; }")
        .arg(circular ? kAvatarSide / 2 : 8);
}

} // namespace

QWidget *PresentationSurface::renderImage(const QJsonObject &payload) {
    const QJsonArray bytes = payload.value(QStringLiteral("data")).toArray();
    QPixmap pixmap;
    if (!bytes.isEmpty()) {
        QByteArray data;
        data.reserve(bytes.size());
        for (const auto &value : bytes) {
            data.append(static_cast<char>(value.toInt()));
        }
        pixmap.loadFromData(data);
    }
    const QString fallback =
        payload.value(QStringLiteral("fallback_text")).toString();
    const bool circular =
        payload.value(QStringLiteral("shape")).toString()
        == QStringLiteral("circle");
    const QJsonObject activation =
        payload.value(QStringLiteral("activation")).toObject();
    if (!activation.isEmpty()) {
        auto *button = new QPushButton;
        button->setText(fallback);
        if (!pixmap.isNull()) {
            button->setIcon(QIcon(pixmap));
            button->setIconSize(QSize(kAvatarSide, kAvatarSide));
        }
        connect(button, &QPushButton::clicked, this,
                [this, activation]() { activate(activation); });
        applyAccessibility(
            button, payload.value(QStringLiteral("accessibility")).toObject());
        return button;
    }
    auto *label = new QLabel;
    if (!pixmap.isNull()) {
        label->setPixmap(
            pixmap.scaled(160, 160, Qt::KeepAspectRatio,
                          Qt::SmoothTransformation));
    } else if (!fallback.isEmpty()) {
        // A `QLabel` with only text paints no body, so the initials sat on
        // the window background and read as a stray letter. The stylesheet
        // carries the whole avatar: a fixed square, a fill and the radius
        // `shape` asks for. Square is load-bearing for the round case — a
        // radius applied to a box that is not square gives a stadium.
        label->setText(fallback);
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumSize(kAvatarSide, kAvatarSide);
        label->setMaximumSize(kAvatarSide, kAvatarSide);
        label->setStyleSheet(avatarStyleSheet(circular));
    }
    applyAccessibility(
        label, payload.value(QStringLiteral("accessibility")).toObject());
    return label;
}

QWidget *PresentationSurface::actionButton(const QJsonObject &action) {
    auto *button =
        new QPushButton(action.value(QStringLiteral("label")).toString());
    button->setObjectName(
        action.value(QStringLiteral("interaction_id")).toString());
    button->setEnabled(action.value(QStringLiteral("enabled")).toBool(true));
    button->setAccessibleName(
        action.value(QStringLiteral("accessibility_label")).toString());
    if (action.value(QStringLiteral("tone")).toString()
        == QStringLiteral("destructive")) {
        button->setProperty("tone", "destructive");
    }
    connect(button, &QPushButton::clicked, this,
            [this, action]() { activate(action); });
    return button;
}

void PresentationSurface::activate(const QJsonObject &action) {
    const QString interaction =
        action.value(QStringLiteral("interaction_id")).toString();
    if (!interaction.isEmpty()) {
        emit interactionReady(m_surfaceId, interaction);
    }
}

void PresentationSurface::applyAccessibility(
    QWidget *widget, const QJsonObject &accessibility) {
    widget->setAccessibleName(
        accessibility.value(QStringLiteral("label")).toString());
    widget->setAccessibleDescription(
        accessibility.value(QStringLiteral("description")).toString());
}
