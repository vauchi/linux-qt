// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include "vauchi.h"

/// Dispatches ExchangeCommands to hardware backends and sends
/// ExchangeHardwareEvents back to core via CABI.
class HardwareBackend : public QObject {
    Q_OBJECT

public:
    explicit HardwareBackend(struct ::VauchiApp *app, QObject *parent = nullptr);

    /// Dispatch a list of exchange commands from an action result.
    /// Returns the action result from any hardware event sent back to core.
    void dispatchCommands(const QJsonArray &commands);

    /// Check which hardware transports are available on this platform.
    bool hasCamera() const;
    bool hasBluetooth() const;

    /// Send a hardware event to core via CABI (used by BleBackend).
    void sendHardwareEvent(const QJsonObject &event);

signals:
    /// Emitted when a hardware event produces a result that needs UI update.
    void actionResultReady(const QJsonObject &result);

    /// Emitted when QR scan completes (camera decoded a QR code).
    void qrScanned(const QString &data);

private:
    void sendUnavailable(const QString &transport);

    struct ::VauchiApp *m_app;
};
