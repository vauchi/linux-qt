// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationsurface.h"

#include "presentationaccessibility.h"
#include "presentationsurface_avatar.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QByteArray>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
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

namespace {

// The design canvas draws a segmented control for the short perspective /
// tab-like choices (2-3 options) and a drop-down for real lists (Settings
// Theme has 15 options).
constexpr int kSegmentedMinOptions = 2;
constexpr int kSegmentedMaxOptions = 3;
const char *const kChoiceIdProperty = "choice_id";

} // namespace

QWidget *PresentationSurface::renderChoice(const QJsonObject &payload) {
    auto *container = new QGroupBox(
        payload.value(QStringLiteral("label")).toString());
    auto *layout = new QVBoxLayout(container);
    const QJsonArray options =
        payload.value(QStringLiteral("options")).toArray();
    const QString binding =
        payload.value(QStringLiteral("binding_id")).toString();
    const QString selected =
        payload.value(QStringLiteral("selected")).toString();
    const bool segmented = options.size() >= kSegmentedMinOptions
                           && options.size() <= kSegmentedMaxOptions;
    QWidget *control = segmented ? segmentedChoice(binding, selected, options)
                                 : comboChoice(binding, selected, options);
    control->setEnabled(payload.value(QStringLiteral("enabled")).toBool(true));
    applyAccessibility(
        control, payload.value(QStringLiteral("accessibility")).toObject());
    control->setObjectName(binding);
    layout->addWidget(control);
    return container;
}

QWidget *PresentationSurface::comboChoice(const QString &binding,
                                          const QString &selected,
                                          const QJsonArray &options) {
    auto *choice = new PresentationChoice;
    for (const auto &optionValue : options) {
        const QJsonObject option = optionValue.toObject();
        choice->addItem(option.value(QStringLiteral("label")).toString(),
                        option.value(QStringLiteral("id")).toString());
    }
    choice->setCurrentIndex(choice->findData(selected));
    connect(choice, &QComboBox::currentIndexChanged, this,
            [this, choice, binding](int) {
                emitChoice(binding, choice->currentData().toString());
            });
    return choice;
}

/// One strip of flat, adjoining, exclusive segments — the strip carries the
/// node's accessible name, each segment its option label.
QWidget *PresentationSurface::segmentedChoice(const QString &binding,
                                              const QString &selected,
                                              const QJsonArray &options) {
    auto *strip = new QWidget;
    auto *layout = new QHBoxLayout(strip);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *group = new QButtonGroup(strip);
    group->setExclusive(true);
    for (int index = 0; index < options.size(); ++index) {
        const QJsonObject option = options.at(index).toObject();
        const QString id = option.value(QStringLiteral("id")).toString();
        const QString label = option.value(QStringLiteral("label")).toString();
        auto *segment = new QToolButton;
        segment->setText(label);
        segment->setAccessibleName(label);
        segment->setToolButtonStyle(Qt::ToolButtonTextOnly);
        segment->setCheckable(true);
        segment->setChecked(id == selected);
        segment->setProperty(kChoiceIdProperty, id);
        segment->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        segment->setMinimumHeight(m_minimumTargetSize);
        segment->setStyleSheet(
            segmentStyleSheet(index == 0, index == options.size() - 1));
        group->addButton(segment);
        layout->addWidget(segment);
    }
    connect(group, &QButtonGroup::buttonToggled, this,
            [this, binding](QAbstractButton *segment, bool checked) {
                if (checked) {
                    emitChoice(binding,
                               segment->property(kChoiceIdProperty).toString());
                }
            });
    return strip;
}

QString PresentationSurface::segmentStyleSheet(bool first, bool last) const {
    // Only the outer corners are rounded, and inner borders are shared, so
    // the segments read as one control rather than a row of buttons.
    QString sheet = QStringLiteral(
        "QToolButton { border: 1px solid palette(mid); border-radius: 0;"
        " padding: 0 12px; background: palette(button); }"
        "QToolButton:checked { background: palette(highlight);"
        " color: palette(highlighted-text); }");
    if (!first) {
        sheet += QStringLiteral("QToolButton { border-left: none; }");
    }
    if (first) {
        sheet += QStringLiteral("QToolButton { border-top-left-radius: %1px;"
                                " border-bottom-left-radius: %1px; }")
                     .arg(m_cornerRadius);
    }
    if (last) {
        sheet += QStringLiteral("QToolButton { border-top-right-radius: %1px;"
                                " border-bottom-right-radius: %1px; }")
                     .arg(m_cornerRadius);
    }
    return sheet;
}

void PresentationSurface::emitChoice(const QString &binding,
                                     const QString &choiceId) {
    emit valueReady(m_surfaceId, binding,
                    QJsonObject{{QStringLiteral("choice"), choiceId}});
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
