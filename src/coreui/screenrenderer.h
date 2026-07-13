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

    /// Renders a ScreenModel JSON directly, bypassing the live engine's
    /// current screen. Used only by the `--render-fixture` catalog harness
    /// (problem 2026-06-12-device-screenshot-catalog) to capture an arbitrary
    /// screen in isolation; the app itself always renders via refresh().
    void renderFixture(const QJsonObject &screen);

signals:
    void screenChanged();

public slots:
    /// Dispatch `UserAction::NavigateBack` through the engine and handle the
    /// result. Called by the visible back affordance and by the system-back
    /// shortcut (ADR-044 Am2a).
    void dispatchNavigateBack();

    /// Dispatch a batch of hardware/commands from an external source (e.g.
    /// the core-driven wakeup tick) through the same path as
    /// `ActionResult::Commands`.
    void dispatchCommands(const QJsonArray &commands);

private:
    void renderScreen(const QJsonObject &screen);
    void handleComponentAction(const QJsonObject &action);
    void processActionResult(const char *resultJson);
    void updateButtonStates();
    void handleAction(const QString &actionId);
    void handleNavAction(const QString &actionId);
    void showValidationError(const QString &componentId, const QString &message);
    void promptQrPaste();
    void saveBackupToFile(const QString &hexData);
    void showStatusMessage(const QString &message);
    void showStatusMessageWithUndo(const QString &message,
                                   const QString &undoActionId);

    struct ::VauchiApp *m_app;
    QVBoxLayout *m_layout;
    QVector<QPair<QString, QPointer<QPushButton>>> m_buttons;
    HardwareBackend *m_hardware = nullptr;
};
