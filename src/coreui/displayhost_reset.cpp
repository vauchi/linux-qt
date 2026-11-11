// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include "applicationmenubar.h"
#include "navigationsidebar.h"
#include "presentationsurface.h"
#include "statusitemtray.h"
#include "thememanager.h"
#include <QApplication>
#include <QDialog>
#include <QToolTip>

void QtDisplayHost::resetApplicationMenu() {
    const auto generation = m_generation;
    m_applicationMenu = new ApplicationMenuBar(this);
    setMenuBar(m_applicationMenu);
    connect(m_applicationMenu, &ApplicationMenuBar::actionActivated, this,
        [this, generation](const QString &id, const QString &interaction) {
            if (generation == m_generation) activate(id, interaction);
        }, Qt::QueuedConnection);
    connect(m_applicationMenu, &ApplicationMenuBar::quitRequested, qApp,
        &QCoreApplication::quit, Qt::QueuedConnection);
}

void QtDisplayHost::resetStatusItem() {
    const auto generation = m_generation;
    m_statusItem = new StatusItemTray(this);
    connect(m_statusItem, &StatusItemTray::actionActivated, this,
        [this, generation](const QString &id, const QString &interaction) {
            if (generation == m_generation) activate(id, interaction);
        }, Qt::QueuedConnection);
    connect(m_statusItem, &StatusItemTray::showWindowRequested, this, &QtDisplayHost::raiseWindow);
    connect(m_statusItem, &StatusItemTray::quitRequested, qApp,
        &QCoreApplication::quit, Qt::QueuedConnection);
}

void QtDisplayHost::resetPresentation() {
    ++m_generation;
    delete m_celebration;
    resetPermissionPrompt();
    resetFileEffects();
    closeOverlay();
    for (auto *dialog : findChildren<QDialog *>()) {
        disconnect(dialog, nullptr, this, nullptr);
        for (auto *child : dialog->findChildren<QObject *>())
            disconnect(child, nullptr, this, nullptr);
        dialog->reject();
    }
    QToolTip::hideText();
    for (auto *widget : QApplication::topLevelWidgets())
        if (widget->windowType() == Qt::ToolTip) widget->hide();
    m_state = PresentationState{};
    for (auto widget : m_surfaces) delete widget;
    m_surfaces.clear();
    delete m_sidebar;
    m_navigation = {};
    m_navigationSurface.clear();
    delete m_applicationMenu;
    resetApplicationMenu();
    m_menuSurface.clear();
    m_menuRevision = -1;
    delete m_statusItem;
    resetStatusItem();
    m_statusSurface.clear();
    m_statusRevision = -1;
    setWindowTitle({});
    ThemeManager::applyTheme(QJsonObject{{"colors", ThemeManager::defaultColors()}}, true);
}
