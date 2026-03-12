// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QSystemTrayIcon>

class QMenu;
class QAction;

class SystemTray : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit SystemTray(QObject *parent = nullptr);

signals:
    void showWindowRequested();
    void quitRequested();
};
