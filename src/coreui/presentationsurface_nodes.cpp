// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationsurface.h"

#include "presentationsurface_avatar.h"

#include <QByteArray>
#include <QJsonArray>
#include <QLabel>
#include <QPalette>
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
        button->setMinimumHeight(m_minimumTargetSize);
        button->setStyleSheet(targetSizeStyleSheet());
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
            button->setIconSize(
                QSize(m_minimumTargetSize, m_minimumTargetSize));
        }
        connect(button, &QPushButton::clicked, this,
                [this, activation]() { activate(activation); });
        applyAccessibility(
            button, payload.value(QStringLiteral("accessibility")).toObject());
        return button;
    }
    auto *label = new QLabel;
    if (!pixmap.isNull() || !fallback.isEmpty()) {
        // vauchi::avatarPixmap() masks real image data to `shape` and paints
        // the fallback initials over a filled ground — a bare QLabel::setText
        // painted no body, so the initials sat on the window background.
        // Square is load-bearing for the round case: a circle clipped from a
        // box that is not square is a stadium.
        label->setPixmap(vauchi::avatarPixmap(
            pixmap, fallback, circular, m_minimumTargetSize, m_cornerRadius,
            palette().color(QPalette::Midlight),
            palette().color(QPalette::Text)));
        label->setMinimumSize(m_minimumTargetSize, m_minimumTargetSize);
        label->setMaximumSize(m_minimumTargetSize, m_minimumTargetSize);
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
    button->setMinimumHeight(m_minimumTargetSize);
    button->setStyleSheet(targetSizeStyleSheet());
    const QString tone = action.value(QStringLiteral("tone")).toString();
    if (tone == QStringLiteral("destructive") || tone == QStringLiteral("serious")) {
        button->setProperty("tone", tone);
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
