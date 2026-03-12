// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QWidget>
#include <QJsonObject>
#include "../componentrenderer.h"

class TextInputComponent {
public:
    static QWidget *render(const QJsonObject &data,
                           const OnAction &onAction = nullptr);
};
