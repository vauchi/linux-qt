// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QWidget>
#include <QJsonObject>
#include <functional>

using OnComponentChanged = std::function<void(const QString &, const QString &)>;

class TextInputComponent {
public:
    static QWidget *render(const QJsonObject &data,
                           const OnComponentChanged &onChange = nullptr);
};
