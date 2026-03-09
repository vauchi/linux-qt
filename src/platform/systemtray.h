// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QSystemTrayIcon>

class QMenu;

/// System tray icon and menu for Vauchi.
class SystemTray : public QSystemTrayIcon {
    Q_OBJECT

public:
    explicit SystemTray(QObject *parent = nullptr);

private:
    QMenu *m_menu = nullptr;
    // TODO: Add tray icon, menu actions (show/hide, quit)
};
