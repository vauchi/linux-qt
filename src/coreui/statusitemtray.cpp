// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusitemtray.h"
#include "applicationmenuaccessibility.h"
#include "navigationicons.h"
#include <QAction>
#include <QIcon>
#include <QJsonArray>
#include <QMenu>
#include <QSystemTrayIcon>

StatusItemTray::StatusItemTray(QObject *parent)
    : QObject(parent), m_icon(new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/vauchi.svg")), this)),
      m_menu(new QMenu), m_showWindow(new QAction(this)), m_quit(new QAction(tr("Quit"), this)) {
    m_quit->setMenuRole(QAction::QuitRole);
    connect(m_showWindow, &QAction::triggered, this, &StatusItemTray::showWindowRequested);
    connect(m_quit, &QAction::triggered, this, &StatusItemTray::quitRequested);
    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) emit showWindowRequested();
    });
    m_icon->setContextMenu(m_menu);
    rebuild({}, {});
}

StatusItemTray::~StatusItemTray() {
    m_icon->hide();
    delete m_menu;
}

bool StatusItemTray::available() { return QSystemTrayIcon::isSystemTrayAvailable(); }

void StatusItemTray::apply(const QJsonObject &payload) {
    const auto item = payload.value("item");
    if (!item.isObject()) {
        m_icon->hide();
        rebuild({}, {});
        return;
    }
    const auto spec = item.toObject();
    m_showWindow->setText(spec.value("show_window_label").toString());
    m_icon->setToolTip(spec.value("tooltip").toString());
    rebuild(payload.value("surface_id").toString(), spec.value("items").toArray());
    if (available()) m_icon->show();
}

void StatusItemTray::withdrawPreparedItems() { rebuild({}, {}); }

void StatusItemTray::rebuild(const QString &surfaceId, const QJsonArray &items) {
    m_menu->clear();
    m_menu->addAction(m_showWindow);
    for (const auto &value : items) {
        const auto item = value.toObject();
        auto *action = new QAction(item.value("label").toString(), m_menu);
        action->setMenuRole(QAction::NoRole);
        action->setEnabled(item.value("enabled").toBool());
        action->setToolTip(item.value("accessibility_label").toString());
        const auto icon = item.value("icon_token").toString();
        if (!icon.isEmpty()) action->setIcon(vauchi::navigationIcon(icon));
        installPreparedMenuActionAccessibility(action, m_menu,
            item.value("accessibility_label").toString());
        connect(action, &QAction::triggered, this, [this, surfaceId, item] {
            emit actionActivated(surfaceId, item.value("interaction_id").toString());
        });
        m_menu->addAction(action);
    }
    m_menu->addSeparator();
    m_menu->addAction(m_quit);
}
