// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <functional>

/// Callback for component actions: passes the full UserAction JSON object.
using OnAction = std::function<void(const QJsonObject &)>;

/// Dispatches component JSON to the appropriate renderer.
class ComponentRenderer {
public:
    static QWidget *render(const QJsonObject &component,
                           const OnAction &onAction = nullptr);
};
