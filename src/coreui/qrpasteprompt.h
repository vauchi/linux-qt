// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>

class QInputDialog;
class QWidget;

/// Non-blocking paste fallback for QrRequestScan when no camera exists.
/// A blocking (exec-style) prompt starves the main event loop and the
/// AT-SPI tree, so this dialog is window-modal via QDialog::open().
class QrPastePrompt : public QObject {
    Q_OBJECT

public:
    explicit QrPastePrompt(QWidget *parent);

    /// Opens the paste dialog and returns immediately. A second call
    /// while the dialog is open raises it instead of stacking dialogs.
    void prompt();

signals:
    /// Exactly one event per closed dialog: QrScanned with the pasted
    /// data, or HardwareUnavailable{"qr_scan"} on cancel/empty accept,
    /// so core never waits on a QrScanned that never arrives (TUI
    /// parity: tui/src/handlers/presentation.rs).
    void eventReady(const QJsonObject &event);

private:
    QWidget *m_parentWidget;
    QPointer<QInputDialog> m_dialog;
};
