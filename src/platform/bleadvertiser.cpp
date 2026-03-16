// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bleadvertiser.h"
#include "hardwarebackend.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMetaType>
#include <QJsonObject>

// ── BleAdvertisementAdaptor ─────────────────────────────────────────

BleAdvertisementAdaptor::BleAdvertisementAdaptor(const QString &serviceUuid,
                                                   QObject *parent)
    : QObject(parent) {
    m_serviceUUIDs << serviceUuid;
}

void BleAdvertisementAdaptor::Release() {
    // BlueZ calls this when the advertisement is released.
    // Nothing to clean up — our parent manages lifetime.
}

// ── BleAdvertiser ───────────────────────────────────────────────────

static const char *kBluezService = "org.bluez";
static const char *kAdvManagerIface = "org.bluez.LEAdvertisingManager1";
static const QString kAdvObjectPath = QStringLiteral("/org/vauchi/advertisement0");

BleAdvertiser::BleAdvertiser(HardwareBackend *backend, QObject *parent)
    : QObject(parent), m_backend(backend) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &BleAdvertiser::stopAdvertising);
}

BleAdvertiser::~BleAdvertiser() {
    stopAdvertising();
}

QString BleAdvertiser::findAdapterPath() {
    // BlueZ registers adapters at /org/bluez/hciN. Try hci0 first.
    QDBusInterface adapter(kBluezService,
                           QStringLiteral("/org/bluez/hci0"),
                           QStringLiteral("org.bluez.Adapter1"),
                           QDBusConnection::systemBus());
    if (adapter.isValid()) {
        return QStringLiteral("/org/bluez/hci0");
    }
    return {};
}

void BleAdvertiser::startAdvertising(const QString &serviceUuid,
                                      const QByteArray &payload) {
    Q_UNUSED(payload);

    // Stop any existing advertisement
    stopAdvertising();

    m_adapterPath = findAdapterPath();
    if (m_adapterPath.isEmpty()) {
        QJsonObject event, inner;
        inner["transport"] = QStringLiteral("BLE");
        inner["error"] = QStringLiteral("No BlueZ adapter found for advertising");
        event["HardwareError"] = inner;
        m_backend->sendHardwareEvent(event);
        return;
    }

    // Create the advertisement D-Bus object
    auto *advObj = new BleAdvertisementAdaptor(serviceUuid, this);

    // Register the object on the system bus
    auto bus = QDBusConnection::systemBus();
    if (!bus.registerObject(kAdvObjectPath, advObj,
                            QDBusConnection::ExportAllSlots
                            | QDBusConnection::ExportAllProperties)) {
        QJsonObject event, inner;
        inner["transport"] = QStringLiteral("BLE");
        inner["error"] = QStringLiteral("Failed to register D-Bus advertisement object");
        event["HardwareError"] = inner;
        m_backend->sendHardwareEvent(event);
        delete advObj;
        return;
    }

    m_advPath = QDBusObjectPath(kAdvObjectPath);

    // Call RegisterAdvertisement on the adapter's LEAdvertisingManager1
    QDBusInterface advManager(kBluezService, m_adapterPath,
                              kAdvManagerIface, bus);
    if (!advManager.isValid()) {
        QJsonObject event, inner;
        inner["transport"] = QStringLiteral("BLE");
        inner["error"] = QStringLiteral("LEAdvertisingManager1 not available on adapter");
        event["HardwareError"] = inner;
        m_backend->sendHardwareEvent(event);
        bus.unregisterObject(kAdvObjectPath);
        delete advObj;
        return;
    }

    QVariantMap options; // empty options dict
    QDBusReply<void> reply = advManager.call(
        QStringLiteral("RegisterAdvertisement"),
        QVariant::fromValue(m_advPath),
        QVariant::fromValue(options));

    if (!reply.isValid()) {
        QJsonObject event, inner;
        inner["transport"] = QStringLiteral("BLE");
        inner["error"] = QStringLiteral("RegisterAdvertisement failed: ")
                         + reply.error().message();
        event["HardwareError"] = inner;
        m_backend->sendHardwareEvent(event);
        bus.unregisterObject(kAdvObjectPath);
        delete advObj;
        return;
    }

    m_registered = true;

    // Auto-stop after 30 seconds (matching linux-gtk behavior)
    m_timer.start(30000);
}

void BleAdvertiser::stopAdvertising() {
    m_timer.stop();

    if (!m_registered) return;

    auto bus = QDBusConnection::systemBus();

    QDBusInterface advManager(kBluezService, m_adapterPath,
                              kAdvManagerIface, bus);
    if (advManager.isValid()) {
        advManager.call(QStringLiteral("UnregisterAdvertisement"),
                        QVariant::fromValue(m_advPath));
    }

    bus.unregisterObject(kAdvObjectPath);
    m_registered = false;
}
