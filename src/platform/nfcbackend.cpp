// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "nfcbackend.h"
#include "hardwarebackend.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>

#include <winscard.h>  // PC/SC API (from pcsclite)

// ── Helpers ─────────────────────────────────────────────────────────

/// Build a SELECT AID APDU: 00 A4 04 00 <len> <AID>
static QByteArray buildSelectApdu(const char *aid, int aidLen) {
    QByteArray apdu;
    apdu.append('\x00'); // CLA
    apdu.append('\xA4'); // INS: SELECT
    apdu.append('\x04'); // P1: Select by DF name
    apdu.append('\x00'); // P2
    apdu.append(static_cast<char>(aidLen)); // Lc
    apdu.append(aid, aidLen);
    return apdu;
}

/// Build an EXCHANGE APDU: 80 E0 00 00 <Lc> <payload> 00
static QByteArray buildExchangeApdu(const QByteArray &payload) {
    QByteArray apdu;
    apdu.append('\x80');   // CLA: proprietary
    apdu.append('\xE0');   // INS: Exchange
    apdu.append('\x00');   // P1
    apdu.append('\x00');   // P2
    apdu.append(static_cast<char>(payload.size() & 0xFF)); // Lc
    apdu.append(payload);
    apdu.append('\x00');   // Le: expect max response
    return apdu;
}

/// Check APDU response status word (last 2 bytes = 90 00 for success).
static bool isApduSuccess(const unsigned char *response, unsigned long len) {
    return len >= 2
        && response[len - 2] == 0x90
        && response[len - 1] == 0x00;
}

// ── NfcBackend ──────────────────────────────────────────────────────

NfcBackend::NfcBackend(HardwareBackend *parent)
    : QObject(nullptr), m_backend(parent) {

    this->moveToThread(&m_workerThread);

    connect(this, &NfcBackend::doPoll, this, [this](QByteArray payload) {
        m_stopRequested.store(false);

        SCARDCONTEXT ctx;
        LONG rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &ctx);
        if (rv != SCARD_S_SUCCESS) {
            QJsonObject event, inner;
            inner["transport"] = QStringLiteral("NFC");
            inner["error"] = QStringLiteral("PC/SC context failed: ")
                             + QString::fromUtf8(pcsc_stringify_error(rv));
            event["HardwareError"] = inner;
            QMetaObject::invokeMethod(m_backend, [this, event]() {
                m_backend->sendHardwareEvent(event);
            }, Qt::QueuedConnection);
            return;
        }

        // Poll for a reader with a card present
        while (!m_stopRequested.load()) {
            DWORD readersLen = 0;
            rv = SCardListReaders(ctx, nullptr, nullptr, &readersLen);
            if (rv != SCARD_S_SUCCESS || readersLen == 0) {
                QThread::msleep(500);
                continue;
            }

            QByteArray readersBuf(static_cast<int>(readersLen), '\0');
            rv = SCardListReaders(ctx, nullptr, readersBuf.data(), &readersLen);
            if (rv != SCARD_S_SUCCESS) {
                QThread::msleep(500);
                continue;
            }

            // Try each reader (multi-string, null-separated, double-null terminated)
            const char *reader = readersBuf.constData();
            while (*reader != '\0' && !m_stopRequested.load()) {
                SCARDHANDLE card;
                DWORD protocol;
                rv = SCardConnect(ctx, reader, SCARD_SHARE_SHARED,
                                  SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                                  &card, &protocol);

                if (rv == SCARD_S_SUCCESS) {
                    const SCARD_IO_REQUEST *pioSend =
                        (protocol == SCARD_PROTOCOL_T1) ? SCARD_PCI_T1 : SCARD_PCI_T0;

                    // Step 1: SELECT Vauchi AID
                    QByteArray selectApdu = buildSelectApdu(kVauchiAid, kVauchiAidLen);
                    unsigned char recvBuf[512];
                    DWORD recvLen = sizeof(recvBuf);

                    rv = SCardTransmit(card, pioSend,
                                       reinterpret_cast<const unsigned char *>(selectApdu.constData()),
                                       static_cast<DWORD>(selectApdu.size()),
                                       nullptr, recvBuf, &recvLen);

                    if (rv == SCARD_S_SUCCESS && isApduSuccess(recvBuf, recvLen)) {
                        // Step 2: EXCHANGE — send our payload, receive theirs
                        QByteArray exchApdu = buildExchangeApdu(payload);
                        recvLen = sizeof(recvBuf);

                        rv = SCardTransmit(card, pioSend,
                                           reinterpret_cast<const unsigned char *>(exchApdu.constData()),
                                           static_cast<DWORD>(exchApdu.size()),
                                           nullptr, recvBuf, &recvLen);

                        if (rv == SCARD_S_SUCCESS && isApduSuccess(recvBuf, recvLen)
                            && recvLen > 2) {
                            // Extract data (response minus SW1 SW2)
                            int dataLen = static_cast<int>(recvLen) - 2;
                            QJsonObject event, inner;
                            QJsonArray dataArr;
                            for (int i = 0; i < dataLen; ++i) {
                                dataArr.append(static_cast<int>(recvBuf[i]));
                            }
                            inner["data"] = dataArr;
                            event["NfcDataReceived"] = inner;

                            QMetaObject::invokeMethod(m_backend, [this, event]() {
                                m_backend->sendHardwareEvent(event);
                            }, Qt::QueuedConnection);

                            SCardDisconnect(card, SCARD_LEAVE_CARD);
                            SCardReleaseContext(ctx);
                            return; // Success — done
                        }
                    }

                    SCardDisconnect(card, SCARD_LEAVE_CARD);
                }

                // Advance to next reader in multi-string
                reader += qstrlen(reader) + 1;
            }

            QThread::msleep(500);
        }

        SCardReleaseContext(ctx);
    });

    m_workerThread.start();
}

NfcBackend::~NfcBackend() {
    m_stopRequested.store(true);
    m_workerThread.quit();
    m_workerThread.wait();
}

void NfcBackend::activate(const QByteArray &payload) {
    emit doPoll(payload);
}

void NfcBackend::deactivate() {
    m_stopRequested.store(true);
}

bool NfcBackend::isAvailable() {
    SCARDCONTEXT ctx;
    LONG rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &ctx);
    if (rv != SCARD_S_SUCCESS) return false;

    DWORD readersLen = 0;
    rv = SCardListReaders(ctx, nullptr, nullptr, &readersLen);
    SCardReleaseContext(ctx);

    return rv == SCARD_S_SUCCESS && readersLen > 0;
}
