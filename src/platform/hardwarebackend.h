// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include "vauchi.h"

class BleBackend;
class AudioBackend;
class NfcBackend;

/// Dispatches ExchangeCommands to hardware backends and sends
/// ExchangeHardwareEvents back to core via CABI.
class HardwareBackend : public QObject {
    Q_OBJECT

public:
    explicit HardwareBackend(struct ::VauchiApp *app, QObject *parent = nullptr);

    /// Dispatch a list of exchange commands from an action result.
    void dispatchCommands(const QJsonArray &commands);

    /// Check which hardware transports are available on this platform.
    bool hasCamera() const;
    bool hasBluetooth() const;
    bool hasAudio() const;
    bool hasNfc() const;

    /// Send a hardware event to core via CABI (used by backends).
    void sendHardwareEvent(const QJsonObject &event);

signals:
    /// Emitted when a hardware event produces a result that needs UI update.
    void actionResultReady(const QJsonObject &result);

    /// Emitted when QR scan completes (camera decoded a QR code).
    void qrScanned(const QString &data);

private:
    void sendUnavailable(const QString &transport);

    /// Extract a QByteArray from a JSON integer array (serde Vec<u8> format).
    static QByteArray jsonArrayToBytes(const QJsonArray &arr);

    struct ::VauchiApp *m_app;

#ifdef VAUCHI_HAS_BLUETOOTH
    BleBackend *m_ble = nullptr;
#endif
#ifdef VAUCHI_HAS_AUDIO
    AudioBackend *m_audio = nullptr;
#endif
#ifdef VAUCHI_HAS_NFC
    NfcBackend *m_nfc = nullptr;
#endif
};
