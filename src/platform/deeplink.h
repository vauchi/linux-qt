// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

namespace vauchi {

/// The `vauchi://` links among the launch arguments (vauchi/private#650).
/// Core decides what a link means; the shell only picks the links out.
QStringList deepLinkUris(const QStringList &args);

/// One running qvauchi per user: a second launch hands its links to the
/// first over a local socket and exits, rather than opening the same data
/// twice.
class DeepLinkRelay : public QObject {
    Q_OBJECT

public:
    explicit DeepLinkRelay(QString serverName, QObject *parent = nullptr);

    /// Sends `links` to a running instance. False when none is running.
    bool handOff(const QStringList &links);

    /// Starts receiving links from later launches.
    bool listen();

    /// The per-user socket name the app uses outside tests.
    static QString defaultServerName();

signals:
    void linkReceived(const QString &uri);

private:
    QString m_serverName;
    QLocalServer *m_server = nullptr;
};

} // namespace vauchi
