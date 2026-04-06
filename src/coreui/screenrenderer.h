// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QVector>
#include <QPointer>
#include "vauchi.h"

class QVBoxLayout;
class QPushButton;
class HardwareBackend;

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
    void handleComponentAction(const QJsonObject &action);
    void processActionResult(const char *resultJson);
    void updateButtonStates();
    void handleAction(const QString &actionId);
    void showValidationError(const QString &componentId, const QString &message);
    void promptQrPaste();
    void showStatusMessage(const QString &message);
    void showStatusMessageWithUndo(const QString &message,
                                   const QString &undoActionId);

    struct ::VauchiApp *m_app;
    QVBoxLayout *m_layout;
    QVector<QPair<QString, QPointer<QPushButton>>> m_buttons;
    HardwareBackend *m_hardware = nullptr;
};
