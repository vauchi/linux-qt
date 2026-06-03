// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QWidget>
#include <QJsonObject>
#include "../componentrenderer.h"

/// Horizontal container component (Component::Row). Renders its `items`
/// left-to-right, each child given equal stretch so a child that fills
/// its own max width (e.g. an ActionList) shares the row instead of
/// overflowing and overlapping its siblings (e.g. a camera preview).
class RowComponent {
public:
    static QWidget *render(const QJsonObject &data,
                           const OnAction &onAction = nullptr);
};
