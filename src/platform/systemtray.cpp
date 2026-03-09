// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemtray.h"
#include <QMenu>

SystemTray::SystemTray(QObject *parent) : QSystemTrayIcon(parent) {
    m_menu = new QMenu;
    setContextMenu(m_menu);
    // TODO: Add show/hide action, quit action, set icon
}
