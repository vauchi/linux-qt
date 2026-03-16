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

#ifdef VAUCHI_HAS_AUDIO
#include "audiobackend.h"
#endif

#ifdef VAUCHI_HAS_NFC
#include "nfcbackend.h"
#endif

HardwareBackend::HardwareBackend(struct VauchiApp *app, QObject *parent)
    : QObject(parent), m_app(app) {
#ifdef VAUCHI_HAS_BLUETOOTH
    if (hasBluetooth()) {
        m_ble = new BleBackend(this);
    }
#endif
#ifdef VAUCHI_HAS_AUDIO
    if (hasAudio()) {
        m_audio = new AudioBackend(this);
    }
#endif
#ifdef VAUCHI_HAS_NFC
    if (hasNfc()) {
        m_nfc = new NfcBackend(this);
    }
#endif
}

void HardwareBackend::dispatchCommands(const QJsonArray &commands) {
    for (const auto &cmd : commands) {
        // Handle unit variants (plain JSON string, e.g. "QrRequestScan")
        if (cmd.isString()) {
            QString variant = cmd.toString();

            if (variant == QLatin1String("QrRequestScan")) {
#ifdef VAUCHI_HAS_CAMERA
                if (hasCamera()) {
                    CameraBackend::startScan(this);
                    continue;
                }
#endif
                emit qrScanned(QString()); // empty = prompt paste
                continue;
            }

            if (variant == QLatin1String("BleDisconnect")) {
#ifdef VAUCHI_HAS_BLUETOOTH
                if (m_ble) {
                    m_ble->disconnect();
                    continue;
                }
#endif
                sendUnavailable("BLE");
                continue;
            }

            if (variant == QLatin1String("NfcDeactivate")) {
#ifdef VAUCHI_HAS_NFC
                if (m_nfc) {
                    m_nfc->deactivate();
                    continue;
                }
#endif
                sendUnavailable("NFC");
                continue;
            }

            if (variant == QLatin1String("AudioStop")) {
#ifdef VAUCHI_HAS_AUDIO
                if (m_audio) {
                    m_audio->stop();
                    continue;
                }
#endif
                continue; // no-op if audio unavailable
            }

            continue;
        }

        QJsonObject cmdObj = cmd.toObject();

        // QrDisplay: handled by screen refresh (QR component renders the data)
        if (cmdObj.contains("QrDisplay")) {
            continue;
        }

        // QrRequestScan (object variant, shouldn't happen but handle defensively)
        if (cmdObj.contains("QrRequestScan")) {
#ifdef VAUCHI_HAS_CAMERA
            if (hasCamera()) {
                CameraBackend::startScan(this);
                continue;
            }
#endif
            emit qrScanned(QString());
            continue;
        }

        // ── BLE commands ────────────────────────────────────────────

        if (cmdObj.contains("BleStartScanning")) {
#ifdef VAUCHI_HAS_BLUETOOTH
            if (m_ble) {
                m_ble->startScanning();
                continue;
            }
#endif
            sendUnavailable("BLE");
            continue;
        }

        if (cmdObj.contains("BleStartAdvertising")) {
#ifdef VAUCHI_HAS_BLUETOOTH
            if (m_ble) {
                QJsonObject inner = cmdObj["BleStartAdvertising"].toObject();
                QString serviceUuid = inner["service_uuid"].toString();
                QByteArray payload = jsonArrayToBytes(inner["payload"].toArray());
                m_ble->startAdvertising(serviceUuid, payload);
                continue;
            }
#endif
            sendUnavailable("BLE");
            continue;
        }

        if (cmdObj.contains("BleConnect")) {
#ifdef VAUCHI_HAS_BLUETOOTH
            if (m_ble) {
                QJsonObject inner = cmdObj["BleConnect"].toObject();
                m_ble->connectDevice(inner["device_id"].toString());
                continue;
            }
#endif
            sendUnavailable("BLE");
            continue;
        }

        if (cmdObj.contains("BleWriteCharacteristic")) {
#ifdef VAUCHI_HAS_BLUETOOTH
            if (m_ble) {
                QJsonObject inner = cmdObj["BleWriteCharacteristic"].toObject();
                QString uuid = inner["uuid"].toString();
                QByteArray data = jsonArrayToBytes(inner["data"].toArray());
                m_ble->writeCharacteristic(uuid, data);
                continue;
            }
#endif
            sendUnavailable("BLE");
            continue;
        }

        if (cmdObj.contains("BleReadCharacteristic")) {
#ifdef VAUCHI_HAS_BLUETOOTH
            if (m_ble) {
                QJsonObject inner = cmdObj["BleReadCharacteristic"].toObject();
                m_ble->readCharacteristic(inner["uuid"].toString());
                continue;
            }
#endif
            sendUnavailable("BLE");
            continue;
        }

        if (cmdObj.contains("BleDisconnect")) {
#ifdef VAUCHI_HAS_BLUETOOTH
            if (m_ble) {
                m_ble->disconnect();
                continue;
            }
#endif
            sendUnavailable("BLE");
            continue;
        }

        // ── Audio commands ──────────────────────────────────────────

        if (cmdObj.contains("AudioEmitChallenge")) {
#ifdef VAUCHI_HAS_AUDIO
            if (m_audio) {
                QJsonObject inner = cmdObj["AudioEmitChallenge"].toObject();
                QByteArray data = jsonArrayToBytes(inner["data"].toArray());
                m_audio->emitChallenge(data);
                continue;
            }
#endif
            sendUnavailable("Audio");
            continue;
        }

        if (cmdObj.contains("AudioListenForResponse")) {
#ifdef VAUCHI_HAS_AUDIO
            if (m_audio) {
                QJsonObject inner = cmdObj["AudioListenForResponse"].toObject();
                uint32_t timeoutMs = static_cast<uint32_t>(inner["timeout_ms"].toDouble());
                m_audio->listenForResponse(timeoutMs);
                continue;
            }
#endif
            sendUnavailable("Audio");
            continue;
        }

        if (cmdObj.contains("AudioStop")) {
#ifdef VAUCHI_HAS_AUDIO
            if (m_audio) {
                m_audio->stop();
                continue;
            }
#endif
            continue;
        }

        // ── NFC commands ────────────────────────────────────────────

        if (cmdObj.contains("NfcActivate")) {
#ifdef VAUCHI_HAS_NFC
            if (m_nfc) {
                QJsonObject inner = cmdObj["NfcActivate"].toObject();
                QByteArray payload = jsonArrayToBytes(inner["payload"].toArray());
                m_nfc->activate(payload);
                continue;
            }
#endif
            sendUnavailable("NFC");
            continue;
        }

        if (cmdObj.contains("NfcDeactivate")) {
#ifdef VAUCHI_HAS_NFC
            if (m_nfc) {
                m_nfc->deactivate();
                continue;
            }
#endif
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

bool HardwareBackend::hasAudio() const {
#ifdef VAUCHI_HAS_AUDIO
    return AudioBackend::isAvailable();
#else
    return false;
#endif
}

bool HardwareBackend::hasNfc() const {
#ifdef VAUCHI_HAS_NFC
    return NfcBackend::isAvailable();
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

QByteArray HardwareBackend::jsonArrayToBytes(const QJsonArray &arr) {
    QByteArray bytes;
    bytes.reserve(arr.size());
    for (const auto &val : arr) {
        bytes.append(static_cast<char>(val.toInt()));
    }
    return bytes;
}
