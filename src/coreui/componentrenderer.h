// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <functional>

/// Callback for component value changes: (component_id, value).
using OnComponentChanged = std::function<void(const QString &, const QString &)>;

/// Dispatches component JSON to the appropriate renderer.
class ComponentRenderer {
public:
    static QWidget *render(const QJsonObject &component,
                           const OnComponentChanged &onChange = nullptr);
};
