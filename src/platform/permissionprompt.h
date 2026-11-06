// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QObject>
#include <QMap>
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
#include <QDBusMessage>
#endif
#include <QVariantMap>

class PermissionPrompt : public QObject {
    Q_OBJECT
public:
    explicit PermissionPrompt(QObject *parent = nullptr);
    ~PermissionPrompt() override;
    void request(const QString &transport);
signals:
    void completed(const QString &event, const QString &transport);
private slots:
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
    void response(uint code, const QVariantMap &results, const QDBusMessage &message);
#endif
private:
    void finish(const QString &event);
    void cancel();
    QString m_transport;
    QString m_path;
    bool m_confirmed = false;
    QMap<QString, uint> m_earlyResponses;
};
