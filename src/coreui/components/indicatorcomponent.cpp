// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indicatorcomponent.h"
#include "../thememanager.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace {

/// Maps the JSON `kind` string to the ThemeRole carrying the matching
/// semantic color. Falls back to StatusNeutral for forward-compat with
/// future variants (the core enum is `#[non_exhaustive]`).
ThemeRole roleFor(const QString &kind) {
    if (kind == QLatin1String("Active")) return ThemeRole::StatusSuccess;
    if (kind == QLatin1String("Error"))  return ThemeRole::StatusError;
    if (kind == QLatin1String("Busy"))   return ThemeRole::StatusInProgress;
    return ThemeRole::StatusNeutral;
}

/// Maps the JSON `kind` string to a freedesktop theme icon name. The
/// `view-refresh` / `dialog-error` etc. names are present in Adwaita
/// and most other icon themes; QIcon::fromTheme falls back to a null
/// icon when missing, which renders as no icon — safe.
QString themeIconFor(const QString &kind) {
    if (kind == QLatin1String("Active")) return QStringLiteral("emblem-default");
    if (kind == QLatin1String("Error"))  return QStringLiteral("dialog-error");
    if (kind == QLatin1String("Busy"))   return QStringLiteral("view-refresh");
    return QStringLiteral("emblem-shared");
}

/// Applies optional `a11y.label` / `a11y.hint` to the rendered widget.
/// Sets accessibleName from `a11y.label` if present, otherwise the
/// visible label; `a11y.hint` lands on accessibleDescription when set.
void applyA11y(QWidget *w, const QJsonObject &data, const QString &label) {
    QString accLabel = label;
    QString accHint;
    if (data.contains(QLatin1String("a11y")) && data["a11y"].isObject()) {
        auto a11y = data["a11y"].toObject();
        auto override_ = a11y.value(QLatin1String("label")).toString();
        if (!override_.isEmpty()) accLabel = override_;
        accHint = a11y.value(QLatin1String("hint")).toString();
    }
    w->setAccessibleName(accLabel);
    if (!accHint.isEmpty()) {
        w->setAccessibleDescription(accHint);
    }
}

}  // namespace

QWidget *IndicatorComponent::render(const QJsonObject &data,
                                    const OnAction &onAction) {
    const QString id          = data[QLatin1String("id")].toString();
    const QString label       = data[QLatin1String("label")].toString();
    const QString kind        = data[QLatin1String("kind")].toString();
    const QString actionId    = data[QLatin1String("action_id")].toString();
    const bool isTappable     = !actionId.isEmpty();
    const QString iconName    = themeIconFor(kind);
    const QString styleSheet  = ThemeManager::styleForRole(roleFor(kind));

    // Per ADR-022: tappable status = QPushButton (carries the affordance
    // discoverably for screen readers + supports keyboard focus);
    // display-only = QLabel/QWidget composite.
    if (isTappable && onAction) {
        auto *btn = new QPushButton(label);
        btn->setObjectName(id);
        btn->setIcon(QIcon::fromTheme(iconName));
        btn->setStyleSheet(styleSheet);
        QObject::connect(btn, &QPushButton::clicked, btn,
                         [onAction, actionId]() {
                             QJsonObject action;
                             QJsonObject inner;
                             inner[QLatin1String("action_id")] = actionId;
                             action[QLatin1String("ActionPressed")] = inner;
                             onAction(action);
                         });
        applyA11y(btn, data, label);
        return btn;
    }

    // Display-only: icon + label in a horizontal chip.
    auto *container = new QWidget;
    container->setObjectName(id);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *iconLabel = new QLabel;
    QIcon icon = QIcon::fromTheme(iconName);
    if (!icon.isNull()) {
        iconLabel->setPixmap(icon.pixmap(16, 16));
    }
    layout->addWidget(iconLabel);

    auto *text = new QLabel(label);
    text->setStyleSheet(styleSheet);
    layout->addWidget(text);
    layout->addStretch();

    applyA11y(container, data, label);
    return container;
}
