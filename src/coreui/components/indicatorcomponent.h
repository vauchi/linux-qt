// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QWidget>
#include <QJsonObject>
#include "../componentrenderer.h"

/// Renders `Component::Indicator` — chrome-positioned ongoing-status
/// chip. Tappable when `action_id` is set; display-only otherwise.
/// Distinct from `StatusIndicatorComponent` (screen-body).
class IndicatorComponent {
public:
    static QWidget *render(const QJsonObject &data,
                           const OnAction &onAction = nullptr);
};
