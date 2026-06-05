// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hardwarebackend.h"
#include "directsendworker.h"

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

        // ── DirectSend (TCP loopback exchange) ──────────────────────

        if (cmdObj.contains("DirectSend")) {
            QJsonObject data = cmdObj["DirectSend"].toObject();
            QByteArray payload = jsonArrayToBytes(data["payload"].toArray());
            bool isInitiator = data["is_initiator"].toBool();

            auto *worker = new DirectSendWorker(payload, isInitiator, this);
            connect(worker, &DirectSendWorker::payloadReceived, this, [this](const QByteArray &received) {
                QJsonObject event;
                QJsonObject inner;
                QJsonArray dataArr;
                for (unsigned char byte : received) {
                    dataArr.append(static_cast<int>(byte));
                }
                inner["data"] = dataArr;
                event["DirectPayloadReceived"] = inner;
                sendHardwareEvent(event);
            });
            connect(worker, &DirectSendWorker::errorOccurred, this, [this](const QString &error) {
                QJsonObject event;
                QJsonObject inner;
                inner["transport"] = QString("USB");
                inner["error"] = error;
                event["HardwareError"] = inner;
                sendHardwareEvent(event);
            });
            connect(worker, &DirectSendWorker::finished, worker, &QObject::deleteLater);
            worker->start();
            continue;
        }

        // ── DirectSendCard (USB card-exchange second leg) ───────────
        // The QR-payload leg above closes its socket, so the card swap runs
        // over a fresh connection. Core decrypts the peer's card under the
        // agreed shared key and completes the exchange.
        if (cmdObj.contains("DirectSendCard")) {
            QJsonObject data = cmdObj["DirectSendCard"].toObject();
            QByteArray ciphertext = jsonArrayToBytes(data["ciphertext"].toArray());
            bool isInitiator = data["is_initiator"].toBool();

            auto *worker = new DirectSendWorker(ciphertext, isInitiator, this);
            connect(worker, &DirectSendWorker::payloadReceived, this, [this](const QByteArray &received) {
                QJsonObject event;
                QJsonObject inner;
                QJsonArray ctArr;
                for (unsigned char byte : received) {
                    ctArr.append(static_cast<int>(byte));
                }
                inner["ciphertext"] = ctArr;
                event["DirectCardReceived"] = inner;
                sendHardwareEvent(event);
            });
            connect(worker, &DirectSendWorker::errorOccurred, this, [this](const QString &error) {
                QJsonObject event;
                QJsonObject inner;
                inner["transport"] = QString("USB");
                inner["error"] = error;
                event["HardwareError"] = inner;
                sendHardwareEvent(event);
            });
            connect(worker, &DirectSendWorker::finished, worker, &QObject::deleteLater);
            worker->start();
            continue;
        }

        // Phase 2b screen-presentation lifecycle commands. Linux desktop
        // has no programmatic brightness control (user owns it via
        // system settings), the OS-level idle timer / screensaver is
        // owned by KDE / GNOME, ShowShareSheet has no system equivalent,
        // and webcams have no front/rear distinction. Answer
        // HardwareUnavailable so core does not retry.
        if (cmdObj.contains("SetScreenBrightness")) {
            sendUnavailable(QStringLiteral("screen_brightness"));
            continue;
        }
        if (cmdObj.contains("SetIdleTimerDisabled")) {
            sendUnavailable(QStringLiteral("idle_timer"));
            continue;
        }
        if (cmdObj.contains("ShowShareSheet")) {
            sendUnavailable(QStringLiteral("share_sheet"));
            continue;
        }
        if (cmdObj.contains("SwitchCamera")) {
            sendUnavailable(QStringLiteral("camera_switch"));
            continue;
        }
        // Phase 2c: orientation lock is a mobile concept — desktop
        // windows are user-resizable and don't rotate with the device.
        if (cmdObj.contains("SetOrientationLock")) {
            sendUnavailable(QStringLiteral("orientation_lock"));
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
