// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemtray.h"
#include <QMenu>
#include <QAction>

SystemTray::SystemTray(QObject *parent) : QSystemTrayIcon(parent) {
    auto *menu = new QMenu;

    auto *showAction = menu->addAction(tr("Show Vauchi"));
    connect(showAction, &QAction::triggered, this, &SystemTray::showWindowRequested);

    menu->addSeparator();

    auto *quitAction = menu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, this, &SystemTray::quitRequested);

    setContextMenu(menu);
    setToolTip("Vauchi");
}
