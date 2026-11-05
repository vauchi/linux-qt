// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationmenubar.h"
#include "applicationmenuaccessibility.h"
#include "navigationicons.h"
#include <QAccessible>
#include <QAction>
#include <QJsonArray>
#include <QKeySequence>
#include <QMenu>

namespace {

QKeySequence shortcut(const QString &token) {
    if (token == "back") return QKeySequence::Back;
    if (token == "undo") return QKeySequence::Undo;
    if (token == "import") return QKeySequence(Qt::CTRL | Qt::Key_I);
    if (token == "activate_primary") return QKeySequence(Qt::CTRL | Qt::Key_Return);
    return {};
}

} // namespace

ApplicationMenuBar::ApplicationMenuBar(QWidget *parent) : QMenuBar(parent) {
    m_quit = new QAction(tr("Quit"), this);
    m_quit->setMenuRole(QAction::QuitRole);
    m_quit->setShortcut(QKeySequence::Quit);
    connect(m_quit, &QAction::triggered, this, &ApplicationMenuBar::quitRequested);
    addAction(m_quit);
}

void ApplicationMenuBar::apply(const QJsonObject &payload) {
    clear();
    for (auto *menu : m_menus) {
        menu->removeAction(m_quit);
        for (auto *action : menu->actions()) action->setEnabled(false);
        menu->close();
        menu->deleteLater();
    }
    m_menus.clear();
    // Qt caches the menu-item owner per QAction, so moving Quit needs a fresh interface (#1073).
    if (auto *accessible = QAccessible::queryAccessibleInterface(m_quit))
        QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(accessible));
    const auto surface = payload.value("surface_id").toString();
    QMenu *file = nullptr;
    for (const auto &value : payload.value("menus").toArray()) {
        const auto section = value.toObject();
        auto *menu = addMenu(section.value("label").toString());
        m_menus.append(menu);
        if (!file && section.value("role").toString() == "file") file = menu;
        for (const auto &itemValue : section.value("items").toArray()) {
            const auto item = itemValue.toObject();
            auto *action = new QAction(item.value("label").toString(), menu);
            action->setMenuRole(QAction::NoRole);
            action->setEnabled(item.value("enabled").toBool());
            action->setToolTip(item.value("accessibility_label").toString());
            action->setShortcut(shortcut(item.value("shortcut").toString()));
            const auto icon = item.value("icon_token").toString();
            if (!icon.isEmpty()) action->setIcon(vauchi::navigationIcon(icon));
            installPreparedMenuActionAccessibility(action, menu,
                item.value("accessibility_label").toString());
            connect(action, &QAction::triggered, this, [this, surface, item] {
                emit actionActivated(surface, item.value("interaction_id").toString());
            });
            menu->addAction(action);
        }
    }
    if (file) {
        if (!file->actions().isEmpty()) file->addSeparator();
        file->addAction(m_quit);
    } else addAction(m_quit);
    // QMenuBar::actionEvent disconnects an action from the bar on removal (#1073).
    connect(m_quit, &QAction::triggered, this, &ApplicationMenuBar::quitRequested,
            Qt::UniqueConnection);
}
