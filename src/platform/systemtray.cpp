// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemtray.h"
#include "../i18n.h"
#include <QMenu>
#include <QAction>
#include <QIcon>

SystemTray::SystemTray(QObject *parent) : QSystemTrayIcon(parent) {
    setIcon(QIcon(QStringLiteral(":/icons/vauchi.svg")));

    auto *menu = new QMenu(qobject_cast<QWidget *>(parent));

    auto *showAction = menu->addAction(tr_vauchi("platform.tray_show", "Show Vauchi"));
    connect(showAction, &QAction::triggered, this, &SystemTray::showWindowRequested);

    menu->addSeparator();

    auto *quitAction = menu->addAction(tr_vauchi("platform.tray_quit", "Quit"));
    connect(quitAction, &QAction::triggered, this, &SystemTray::quitRequested);

    setContextMenu(menu);
    setToolTip("Vauchi");
}
