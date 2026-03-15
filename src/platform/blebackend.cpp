// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "blebackend.h"
#include "hardwarebackend.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyService>

BleBackend::BleBackend(HardwareBackend *parent)
    : QObject(parent), m_backend(parent) {
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(10000); // 10s timeout

    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BleBackend::onDeviceDiscovered);
}

void BleBackend::startScanning() {
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void BleBackend::stopScanning() {
    m_discoveryAgent->stop();
}

void BleBackend::onDeviceDiscovered(const QBluetoothDeviceInfo &info) {
    // Only report BLE devices
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
        return;
    }

    // Send BleDeviceDiscovered event to core
    QJsonObject event;
    QJsonObject inner;
    inner["id"] = info.address().toString();
    inner["rssi"] = info.rssi();

    // Convert manufacturer data to adv_data bytes
    QJsonArray advData;
    for (auto key : info.manufacturerIds()) {
        QByteArray data = info.manufacturerData(key);
        for (char byte : data) {
            advData.append(static_cast<int>(static_cast<unsigned char>(byte)));
        }
    }
    inner["adv_data"] = advData;
    event["BleDeviceDiscovered"] = inner;

    m_backend->sendHardwareEvent(event);
}

void BleBackend::connectDevice(const QString &deviceId) {
    QBluetoothAddress addr(deviceId);
    QBluetoothDeviceInfo deviceInfo(addr, QString(), 0);

    m_controller = QLowEnergyController::createCentral(deviceInfo, this);

    connect(m_controller, &QLowEnergyController::connected, this, [this, deviceId]() {
        QJsonObject event;
        QJsonObject inner;
        inner["device_id"] = deviceId;
        event["BleConnected"] = inner;
        m_backend->sendHardwareEvent(event);
    });

    connect(m_controller, &QLowEnergyController::disconnected, this, [this]() {
        QJsonObject event;
        QJsonObject inner;
        inner["reason"] = QStringLiteral("disconnected");
        event["BleDisconnected"] = inner;
        m_backend->sendHardwareEvent(event);
    });

    m_controller->connectToDevice();
}

void BleBackend::writeCharacteristic(const QString &uuid, const QByteArray &data) {
    Q_UNUSED(uuid);
    Q_UNUSED(data);
    // TODO: Implement GATT characteristic write once service discovery is wired
}

void BleBackend::readCharacteristic(const QString &uuid) {
    Q_UNUSED(uuid);
    // TODO: Implement GATT characteristic read once service discovery is wired
}

void BleBackend::disconnect() {
    if (m_controller) {
        m_controller->disconnectFromDevice();
    }
}
