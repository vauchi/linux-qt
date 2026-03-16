// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QTimer>

class HardwareBackend;

/// BLE peripheral advertising via BlueZ D-Bus API.
///
/// Qt Bluetooth does not support BLE peripheral mode on Linux.
/// This class uses the BlueZ LEAdvertisingManager1 D-Bus interface
/// directly to register a BLE advertisement with the system's
/// Bluetooth adapter.
///
/// The advertisement exposes the vauchi service UUID and local name
/// so that scanning devices can discover this desktop.
class BleAdvertiser : public QObject {
    Q_OBJECT

public:
    explicit BleAdvertiser(HardwareBackend *backend, QObject *parent = nullptr);
    ~BleAdvertiser() override;

    /// Start advertising the given service UUID with payload.
    /// Automatically stops after 30 seconds.
    void startAdvertising(const QString &serviceUuid, const QByteArray &payload);

    /// Stop advertising and unregister the advertisement.
    void stopAdvertising();

private:
    /// Find the default BlueZ adapter path (e.g., /org/bluez/hci0).
    QString findAdapterPath();

    HardwareBackend *m_backend;
    QString m_adapterPath;
    QDBusObjectPath m_advPath;
    bool m_registered = false;
    QTimer m_timer;
};

/// D-Bus adaptor that exposes org.bluez.LEAdvertisement1 properties.
/// BlueZ reads these properties when the advertisement is registered.
class BleAdvertisementAdaptor : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.LEAdvertisement1")

    Q_PROPERTY(QString Type READ type)
    Q_PROPERTY(QStringList ServiceUUIDs READ serviceUUIDs)
    Q_PROPERTY(QString LocalName READ localName)

public:
    explicit BleAdvertisementAdaptor(const QString &serviceUuid, QObject *parent = nullptr);

    QString type() const { return QStringLiteral("peripheral"); }
    QStringList serviceUUIDs() const { return m_serviceUUIDs; }
    QString localName() const { return QStringLiteral("Vauchi"); }

public slots:
    /// Called by BlueZ when the advertisement is released.
    Q_SCRIPTABLE void Release();

private:
    QStringList m_serviceUUIDs;
};
