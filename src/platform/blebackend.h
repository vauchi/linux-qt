// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QJsonObject>

class HardwareBackend;

/// BLE backend using Qt Bluetooth for device discovery and GATT operations.
class BleBackend : public QObject {
    Q_OBJECT

public:
    explicit BleBackend(HardwareBackend *parent);

    void startScanning();
    void stopScanning();
    void connectDevice(const QString &deviceId);
    void writeCharacteristic(const QString &uuid, const QByteArray &data);
    void readCharacteristic(const QString &uuid);
    void disconnect();

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);

private:
    HardwareBackend *m_backend;
    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent = nullptr;
    QLowEnergyController *m_controller = nullptr;
};
