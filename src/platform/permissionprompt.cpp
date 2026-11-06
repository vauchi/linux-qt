// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "permissionprompt.h"
#include <utility>
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
#include <QDBusConnection>
#include <QPointer>
#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QUuid>
namespace {
const QString service = QStringLiteral("org.freedesktop.portal.Desktop");
const QString base = QStringLiteral("/org/freedesktop/portal/desktop");
const QString requestInterface = QStringLiteral("org.freedesktop.portal.Request");
}
#endif

PermissionPrompt::PermissionPrompt(QObject *parent) : QObject(parent) {
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
    auto *watcher = new QDBusServiceWatcher(service, QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        finish(QStringLiteral("HardwareUnavailable"));
    });
#endif
}
PermissionPrompt::~PermissionPrompt() { cancel(); }

void PermissionPrompt::request(const QString &transport) {
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
    if (transport.compare(QStringLiteral("camera"), Qt::CaseInsensitive) == 0) {
        if (!m_transport.isEmpty()) return;
        auto bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            emit completed(QStringLiteral("HardwareUnavailable"), transport);
            return;
        }
        m_transport = transport;
        QString sender = bus.baseService().mid(1); sender.replace('.', '_');
        const QString token = QStringLiteral("vauchi_") + QUuid::createUuid().toString(QUuid::Id128);
        m_path = base + "/request/" + sender + "/" + token;
        // Subscribe before the call because a portal may answer before its method reply.
        if (!bus.connect(service, {}, requestInterface, QStringLiteral("Response"), this,
                         SLOT(response(uint,QVariantMap,QDBusMessage)))) {
            finish(QStringLiteral("HardwareUnavailable"));
            return;
        }
        auto message = QDBusMessage::createMethodCall(service, base,
            QStringLiteral("org.freedesktop.portal.Camera"), QStringLiteral("AccessCamera"));
        message << QVariantMap{{QStringLiteral("handle_token"), token}};
        auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), qApp);
        const QPointer<PermissionPrompt> owner(this);
        connect(watcher, &QDBusPendingCallWatcher::finished, qApp, [owner, watcher, path = m_path] {
            const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            watcher->deleteLater();
            const QString prefix = path.left(path.lastIndexOf('/') + 1);
            if (!owner || owner->m_path != path) {
                if (!reply.isError() && reply.value().path() != path
                    && reply.value().path().startsWith(prefix))
                    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(service,
                        reply.value().path(), requestInterface, QStringLiteral("Close")));
                return;
            }
            if (reply.isError() || !reply.value().path().startsWith(prefix)) {
                owner->cancel();
                owner->finish(QStringLiteral("HardwareUnavailable"));
                return;
            }
            owner->m_path = reply.value().path();
            owner->m_confirmed = true;
            const auto early = owner->m_earlyResponses.constFind(owner->m_path);
            if (early != owner->m_earlyResponses.cend()) {
                const uint code = early.value();
                owner->finish(code == 0 ? QStringLiteral("PermissionGranted")
                    : code == 1 ? QStringLiteral("PermissionDenied") : QStringLiteral("HardwareUnavailable"));
            }
            owner->m_earlyResponses.clear();
        });
        return;
    }
#endif
    emit completed(QStringLiteral("HardwareUnavailable"), transport);
}

#ifdef VAUCHI_HAS_PERMISSION_PORTAL
void PermissionPrompt::response(uint code, const QVariantMap &, const QDBusMessage &message) {
    if (m_transport.isEmpty()) return;
    if (!m_confirmed) {
        if (!message.path().startsWith(m_path.left(m_path.lastIndexOf('/') + 1))) return;
        if (m_earlyResponses.size() >= 16 && !m_earlyResponses.contains(message.path())) {
            cancel();
            finish(QStringLiteral("HardwareUnavailable"));
        } else m_earlyResponses.insert(message.path(), code);
        return;
    }
    if (message.path() != m_path) return;
    finish(code == 0 ? QStringLiteral("PermissionGranted")
        : code == 1 ? QStringLiteral("PermissionDenied") : QStringLiteral("HardwareUnavailable"));
}
#endif

void PermissionPrompt::finish(const QString &event) {
    if (m_transport.isEmpty()) return;
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
    QDBusConnection::sessionBus().disconnect(service, {}, requestInterface,
        QStringLiteral("Response"), this, SLOT(response(uint,QVariantMap,QDBusMessage)));
#endif
    m_path.clear();
    m_confirmed = false;
    m_earlyResponses.clear();
    const QString transport = std::exchange(m_transport, {});
    emit completed(event, transport);
}

void PermissionPrompt::cancel() {
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
    if (!m_path.isEmpty()) {
        auto bus = QDBusConnection::sessionBus();
        bus.disconnect(service, {}, requestInterface, QStringLiteral("Response"),
            this, SLOT(response(uint,QVariantMap,QDBusMessage)));
        bus.asyncCall(QDBusMessage::createMethodCall(service, m_path, requestInterface, QStringLiteral("Close")));
    }
#endif
    m_path.clear();
}
