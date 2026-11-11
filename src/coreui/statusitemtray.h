// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QObject>

class QAction;
class QMenu;
class QSystemTrayIcon;

class StatusItemTray : public QObject {
    Q_OBJECT
public:
    explicit StatusItemTray(QObject *parent);
    ~StatusItemTray() override;
    static bool available();
    void apply(const QJsonObject &payload);
    void withdrawPreparedItems();

signals:
    void actionActivated(const QString &surfaceId, const QString &interactionId);
    void showWindowRequested();
    void quitRequested();

private:
    void rebuild(const QString &surfaceId, const QJsonArray &items);

    QSystemTrayIcon *m_icon;
    QMenu *m_menu;
    QAction *m_showWindow;
    QAction *m_quit;
};
