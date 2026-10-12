// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "deeplink.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <unistd.h>

namespace vauchi {

namespace {
const QString kSchemePrefix = QStringLiteral("vauchi://");
constexpr int kHandOffTimeoutMs = 1000;
constexpr qint64 kMaxHandOffBytes = 64 * 1024;
} // namespace

QStringList deepLinkUris(const QStringList &args) {
    QStringList links;
    for (const QString &arg : args) {
        if (arg.startsWith(kSchemePrefix, Qt::CaseInsensitive)) links.append(arg);
    }
    return links;
}

DeepLinkRelay::DeepLinkRelay(QString serverName, QObject *parent)
    : QObject(parent), m_serverName(std::move(serverName)) {}

QString DeepLinkRelay::defaultServerName() {
    return QStringLiteral("vauchi-qt-%1").arg(getuid());
}

bool DeepLinkRelay::handOff(const QStringList &links) {
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    if (!socket.waitForConnected(kHandOffTimeoutMs)) return false;
    socket.write(links.join('\n').toUtf8() + '\n');
    socket.waitForBytesWritten(kHandOffTimeoutMs);
    socket.disconnectFromServer();
    return true;
}

bool DeepLinkRelay::listen() {
    // A crashed instance leaves its socket file behind; listening fails on
    // it until it is removed.
    QLocalServer::removeServer(m_serverName);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server->nextPendingConnection()) {
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
            connect(client, &QLocalSocket::readyRead, this, [this, client] {
                if (client->bytesAvailable() > kMaxHandOffBytes) {
                    client->abort();
                    return;
                }
                while (client->canReadLine()) {
                    const QString line = QString::fromUtf8(client->readLine()).trimmed();
                    for (const QString &uri : deepLinkUris({line})) emit linkReceived(uri);
                }
            });
        }
    });
    return m_server->listen(m_serverName);
}

} // namespace vauchi
