// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QVector>
#include "vauchi.h"

class QVBoxLayout;
class QPushButton;

/// Renders a ScreenModel JSON into Qt widgets.
class ScreenRenderer : public QWidget {
    Q_OBJECT

public:
    explicit ScreenRenderer(struct ::VauchiApp *app, QWidget *parent = nullptr);
    void refresh();

signals:
    void screenChanged();

private:
    void renderScreen(const QJsonObject &screen);
    void handleTextChanged(const QString &componentId, const QString &value);
    void updateButtonStates();
    void handleAction(const QString &actionId);

    struct ::VauchiApp *m_app;
    QVBoxLayout *m_layout;
    QVector<QPair<QString, QPushButton*>> m_buttons;
};
