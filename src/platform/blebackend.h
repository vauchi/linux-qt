// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QJsonObject>
#include <QHash>

class HardwareBackend;
class BleAdvertiser;

/// BLE backend using Qt Bluetooth for device discovery, connection, and GATT operations.
///
/// After connecting to a device, automatically discovers services and caches
/// characteristics for read/write by UUID. Characteristic notifications are
/// forwarded as BleCharacteristicNotified events to core.
class BleBackend : public QObject {
    Q_OBJECT

public:
    explicit BleBackend(HardwareBackend *parent);

    void startScanning();
    void stopScanning();
    void startAdvertising(const QString &serviceUuid, const QByteArray &payload);
    void connectDevice(const QString &deviceId);
    void writeCharacteristic(const QString &uuid, const QByteArray &data);
    void readCharacteristic(const QString &uuid);
    void disconnect();

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onServiceDiscovered(const QBluetoothUuid &serviceUuid);
    void onServiceDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onCharacteristicRead(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void onCharacteristicWritten(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);

private:
    /// Find a cached characteristic by UUID string across all discovered services.
    QLowEnergyCharacteristic findCharacteristic(const QString &uuid) const;

    HardwareBackend *m_backend;
    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent = nullptr;
    QLowEnergyController *m_controller = nullptr;
    BleAdvertiser *m_advertiser = nullptr;

    /// All discovered services, keyed by service UUID.
    QHash<QBluetoothUuid, QLowEnergyService *> m_services;
};
