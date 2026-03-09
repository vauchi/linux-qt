// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QJsonObject>

/// Dispatches component JSON to the appropriate renderer.
class ComponentRenderer {
public:
    static QWidget *render(const QJsonObject &component);
};
