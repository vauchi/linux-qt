// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QJsonObject>
#include "vauchi.h"

class QVBoxLayout;

/// Renders a ScreenModel JSON into Qt widgets.
class ScreenRenderer : public QWidget {
    Q_OBJECT

public:
    explicit ScreenRenderer(struct ::VauchiApp *app, QWidget *parent = nullptr);
    void refresh();

private:
    void renderScreen(const QJsonObject &screen);
    void handleAction(const QString &actionId);

    struct ::VauchiApp *m_app;
    QVBoxLayout *m_layout;
};
