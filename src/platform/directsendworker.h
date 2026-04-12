// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QThread>

/// TCP worker that performs a VXCH-framed exchange on a background thread.
///
/// VXCH frame format:
///   [4] magic "VXCH"
///   [1] version = 1
///   [4] payload length (big-endian uint32)
///   [N] payload bytes
///
/// Initiator: connects to 127.0.0.1:DefaultPort, sends frame, reads response.
/// Responder: listens on DefaultPort, accepts one connection, reads frame, sends response.
class DirectSendWorker : public QThread {
    Q_OBJECT
public:
    static constexpr quint16 DefaultPort = 19283;

    DirectSendWorker(const QByteArray &payload, bool isInitiator, QObject *parent = nullptr);

signals:
    void payloadReceived(const QByteArray &data);
    void errorOccurred(const QString &error);

protected:
    void run() override;

private:
    QByteArray m_payload;
    bool m_isInitiator;

    void runInitiator();
    void runResponder();

    bool sendVxch(int sock, const QByteArray &payload);
    QByteArray recvVxch(int sock);
    bool sendAll(int sock, const char *data, int len);
    QByteArray recvExact(int sock, int count);
};
