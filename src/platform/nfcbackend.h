// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QByteArray>
#include <QThread>
#include <atomic>

class HardwareBackend;

/// NFC backend using PC/SC (pcsclite) for USB NFC reader exchange.
///
/// On desktop Linux, NFC exchange uses USB NFC readers (ACR122U, PN532)
/// via the PC/SC API (pcsclite daemon). The desktop acts as a reader
/// and exchanges data with a phone doing NFC Host Card Emulation (HCE).
///
/// APDU protocol:
///   1. SELECT Vauchi AID (F0564155434849)
///   2. EXCHANGE command (INS 0xE0) — send our payload, receive theirs
///
/// All PC/SC operations run on a worker thread to avoid blocking the UI.
class NfcBackend : public QObject {
    Q_OBJECT

public:
    explicit NfcBackend(HardwareBackend *parent);
    ~NfcBackend() override;

    /// Activate NFC: start polling for cards and exchange `payload` on tap.
    void activate(const QByteArray &payload);

    /// Deactivate NFC: stop polling.
    void deactivate();

    /// Check if any NFC reader is present via PC/SC.
    static bool isAvailable();

signals:
    /// Internal: start polling on worker thread.
    void doPoll(QByteArray payload);

private:
    /// Vauchi NFC Application ID (SELECT AID).
    static constexpr const char *kVauchiAid = "\xF0\x56\x41\x55\x43\x48\x49";
    static constexpr int kVauchiAidLen = 7;

    /// APDU instruction for payload exchange.
    static constexpr unsigned char kInsExchange = 0xE0;

    HardwareBackend *m_backend;
    QThread m_workerThread;
    std::atomic<bool> m_stopRequested{false};
};
