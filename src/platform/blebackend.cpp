// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "blebackend.h"
#include "bleadvertiser.h"
#include "hardwarebackend.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyService>
#include <QLowEnergyDescriptor>

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

void BleBackend::startAdvertising(const QString &serviceUuid, const QByteArray &payload) {
    if (!m_advertiser) {
        m_advertiser = new BleAdvertiser(m_backend, this);
    }
    m_advertiser->startAdvertising(serviceUuid, payload);
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
    // Clean up previous connection
    if (m_controller) {
        m_controller->disconnectFromDevice();
        m_controller->deleteLater();
        m_controller = nullptr;
    }
    qDeleteAll(m_services);
    m_services.clear();

    QBluetoothAddress addr(deviceId);
    QBluetoothDeviceInfo deviceInfo(addr, QString(), 0);

    m_controller = QLowEnergyController::createCentral(deviceInfo, this);

    connect(m_controller, &QLowEnergyController::connected, this, [this, deviceId]() {
        QJsonObject event;
        QJsonObject inner;
        inner["device_id"] = deviceId;
        event["BleConnected"] = inner;
        m_backend->sendHardwareEvent(event);

        // Trigger service discovery after successful connection
        m_controller->discoverServices();
    });

    connect(m_controller, &QLowEnergyController::disconnected, this, [this]() {
        QJsonObject event;
        QJsonObject inner;
        inner["reason"] = QStringLiteral("disconnected");
        event["BleDisconnected"] = inner;
        m_backend->sendHardwareEvent(event);
    });

    connect(m_controller, &QLowEnergyController::serviceDiscovered,
            this, &BleBackend::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished,
            this, &BleBackend::onServiceDiscoveryFinished);

    m_controller->connectToDevice();
}

void BleBackend::onServiceDiscovered(const QBluetoothUuid &serviceUuid) {
    Q_UNUSED(serviceUuid);
    // Services are collected; details discovered in onServiceDiscoveryFinished.
}

void BleBackend::onServiceDiscoveryFinished() {
    // Discover details for each service to populate characteristics
    const auto serviceUuids = m_controller->services();
    for (const auto &uuid : serviceUuids) {
        auto *service = m_controller->createServiceObject(uuid, this);
        if (!service) continue;

        m_services.insert(uuid, service);

        connect(service, &QLowEnergyService::stateChanged,
                this, &BleBackend::onServiceStateChanged);
        connect(service, &QLowEnergyService::characteristicRead,
                this, &BleBackend::onCharacteristicRead);
        connect(service, &QLowEnergyService::characteristicWritten,
                this, &BleBackend::onCharacteristicWritten);
        connect(service, &QLowEnergyService::characteristicChanged,
                this, &BleBackend::onCharacteristicChanged);

        service->discoverDetails();
    }
}

void BleBackend::onServiceStateChanged(QLowEnergyService::ServiceState state) {
    if (state != QLowEnergyService::RemoteServiceDiscovered) return;

    auto *service = qobject_cast<QLowEnergyService *>(sender());
    if (!service) return;

    // Enable notifications for all characteristics that support it
    const auto chars = service->characteristics();
    for (const auto &c : chars) {
        if (c.properties() & QLowEnergyCharacteristic::Notify) {
            auto cccd = c.descriptor(
                QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
            if (cccd.isValid()) {
                service->writeDescriptor(cccd, QLowEnergyCharacteristic::CCCDEnableNotification);
            }
        }
    }
}

void BleBackend::onCharacteristicRead(const QLowEnergyCharacteristic &c,
                                       const QByteArray &value) {
    QJsonObject event;
    QJsonObject inner;
    inner["uuid"] = c.uuid().toString(QUuid::WithoutBraces);

    QJsonArray data;
    for (char byte : value) {
        data.append(static_cast<int>(static_cast<unsigned char>(byte)));
    }
    inner["data"] = data;
    event["BleCharacteristicRead"] = inner;

    m_backend->sendHardwareEvent(event);
}

void BleBackend::onCharacteristicWritten(const QLowEnergyCharacteristic &c,
                                          const QByteArray &value) {
    Q_UNUSED(c);
    Q_UNUSED(value);
    // Write confirmed — no event needed back to core for writes.
}

void BleBackend::onCharacteristicChanged(const QLowEnergyCharacteristic &c,
                                          const QByteArray &value) {
    QJsonObject event;
    QJsonObject inner;
    inner["uuid"] = c.uuid().toString(QUuid::WithoutBraces);

    QJsonArray data;
    for (char byte : value) {
        data.append(static_cast<int>(static_cast<unsigned char>(byte)));
    }
    inner["data"] = data;
    event["BleCharacteristicNotified"] = inner;

    m_backend->sendHardwareEvent(event);
}

QLowEnergyCharacteristic BleBackend::findCharacteristic(const QString &uuid) const {
    QBluetoothUuid target(uuid);
    for (auto *service : m_services) {
        if (service->state() != QLowEnergyService::RemoteServiceDiscovered) continue;
        const auto chars = service->characteristics();
        for (const auto &c : chars) {
            if (c.uuid() == target) {
                return c;
            }
        }
    }
    return QLowEnergyCharacteristic(); // invalid
}

void BleBackend::writeCharacteristic(const QString &uuid, const QByteArray &data) {
    auto c = findCharacteristic(uuid);
    if (!c.isValid()) {
        QJsonObject event;
        QJsonObject inner;
        inner["transport"] = QStringLiteral("BLE");
        inner["error"] = QStringLiteral("Characteristic not found: ") + uuid;
        event["HardwareError"] = inner;
        m_backend->sendHardwareEvent(event);
        return;
    }

    // Find the owning service
    for (auto *service : m_services) {
        if (service->state() != QLowEnergyService::RemoteServiceDiscovered) continue;
        const auto chars = service->characteristics();
        for (const auto &sc : chars) {
            if (sc.uuid() == c.uuid()) {
                auto mode = (c.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                    ? QLowEnergyService::WriteWithoutResponse
                    : QLowEnergyService::WriteWithResponse;
                service->writeCharacteristic(c, data, mode);
                return;
            }
        }
    }
}

void BleBackend::readCharacteristic(const QString &uuid) {
    auto c = findCharacteristic(uuid);
    if (!c.isValid()) {
        QJsonObject event;
        QJsonObject inner;
        inner["transport"] = QStringLiteral("BLE");
        inner["error"] = QStringLiteral("Characteristic not found: ") + uuid;
        event["HardwareError"] = inner;
        m_backend->sendHardwareEvent(event);
        return;
    }

    for (auto *service : m_services) {
        if (service->state() != QLowEnergyService::RemoteServiceDiscovered) continue;
        const auto chars = service->characteristics();
        for (const auto &sc : chars) {
            if (sc.uuid() == c.uuid()) {
                service->readCharacteristic(c);
                return;
            }
        }
    }
}

void BleBackend::disconnect() {
    if (m_controller) {
        m_controller->disconnectFromDevice();
    }
}
