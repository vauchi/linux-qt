// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indicatorcomponent.h"
#include <QWidget>

// RED stub: real implementation lands in the follow-up feat: commit.
QWidget *IndicatorComponent::render(const QJsonObject &data,
                                    const OnAction & /*onAction*/) {
    auto *placeholder = new QWidget;
    placeholder->setObjectName(data["id"].toString());
    return placeholder;
}
