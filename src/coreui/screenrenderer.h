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
    void saveBackupToFile(const QString &hexData);
    void showStatusMessage(const QString &message);
    void showStatusMessageWithUndo(const QString &message,
                                   const QString &undoActionId);

    struct ::VauchiApp *m_app;
    // Outer layout owns either a QScrollArea (default "Scroll" layout) or
    // the content widget directly ("Fixed" layout, e.g. the QR exchange
    // screen — a moving/reflowing QR breaks the peer camera's lock).
    QVBoxLayout *m_layout;
    // Layout the per-screen widgets (title, components, actions) are added
    // to. Lives inside the scroll area's inner widget when scrollable, so
    // showValidationError() can locate a target widget's row regardless of
    // the scroll wrapping.
    QVBoxLayout *m_contentLayout = nullptr;
    QVector<QPair<QString, QPointer<QPushButton>>> m_buttons;
    HardwareBackend *m_hardware = nullptr;
};
