// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hardwarebackend.h"

#include <QJsonDocument>
#include <QDir>

#ifdef VAUCHI_HAS_CAMERA
#include "camerabackend.h"
#include <QMediaDevices>
#endif

#ifdef VAUCHI_HAS_BLUETOOTH
#include "blebackend.h"
#endif

HardwareBackend::HardwareBackend(struct VauchiApp *app, QObject *parent)
    : QObject(parent), m_app(app) {}

void HardwareBackend::dispatchCommands(const QJsonArray &commands) {
    for (const auto &cmd : commands) {
        QJsonObject cmdObj = cmd.toObject();

        // QrDisplay: handled by screen refresh (QR component renders the data)
        if (cmdObj.contains("QrDisplay")) {
            continue;
        }

        // QrRequestScan: attempt camera, fall back to paste dialog
        if (cmdObj.contains("QrRequestScan")) {
#ifdef VAUCHI_HAS_CAMERA
            if (hasCamera()) {
                CameraBackend::startScan(this);
                continue;
            }
#endif
            // No camera — frontend should show paste dialog
            // (handled by ScreenRenderer::promptQrPaste via signal)
            emit qrScanned(QString()); // empty = prompt paste
            continue;
        }

        // BLE commands
        if (cmdObj.contains("BleStartScanning") || cmdObj.contains("BleStartAdvertising")) {
#ifdef VAUCHI_HAS_BLUETOOTH
            if (hasBluetooth()) {
                // BLE backend handles these
                continue;
            }
#endif
            sendUnavailable("BLE");
            continue;
        }

        if (cmdObj.contains("BleConnect")) {
#ifdef VAUCHI_HAS_BLUETOOTH
            // Forward to BLE backend
            continue;
#else
            sendUnavailable("BLE");
            continue;
#endif
        }

        // Audio commands
        if (cmdObj.contains("AudioEmitChallenge") || cmdObj.contains("AudioListenForResponse")) {
            sendUnavailable("Audio");
            continue;
        }

        // NFC commands
        if (cmdObj.contains("NfcActivate")) {
            sendUnavailable("NFC");
            continue;
        }
    }
}

bool HardwareBackend::hasCamera() const {
#ifdef VAUCHI_HAS_CAMERA
    return !QMediaDevices::videoInputs().isEmpty();
#else
    return false;
#endif
}

bool HardwareBackend::hasBluetooth() const {
#ifdef VAUCHI_HAS_BLUETOOTH
    return QDir("/sys/class/bluetooth").exists()
           && !QDir("/sys/class/bluetooth").entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();
#else
    return false;
#endif
}

void HardwareBackend::sendHardwareEvent(const QJsonObject &event) {
    QByteArray json = QJsonDocument(event).toJson(QJsonDocument::Compact);
    char *result = vauchi_app_handle_hardware_event(m_app, json.constData());
    if (result) {
        QJsonDocument doc = QJsonDocument::fromJson(result);
        if (doc.isObject()) {
            emit actionResultReady(doc.object());
        }
        vauchi_string_free(result);
    }
}

void HardwareBackend::sendUnavailable(const QString &transport) {
    QJsonObject event;
    QJsonObject inner;
    inner["transport"] = transport;
    event["HardwareUnavailable"] = inner;
    sendHardwareEvent(event);
}
