// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menubar.h"
#include "../i18n.h"
#include <QAction>
#include <QKeySequence>
#include <QMessageBox>
#include <QJsonArray>
#include <QMenu>

void VauchiMenuBar::applyApplicationMenu(const QJsonObject &payload) {
    for (QAction *action : m_preparedActions) {
        action->setEnabled(false);
        if (auto *owner = qobject_cast<QMenu *>(action->parent())) {
            owner->removeAction(action);
        }
        // A batch can arrive synchronously from this action's triggered signal.
        action->deleteLater();
    }
    m_preparedActions.clear();
    for (QMenu *menu : m_preparedMenus) {
        removeAction(menu->menuAction());
        menu->deleteLater();
    }
    m_preparedMenus.clear();
    const QString surfaceId = payload.value(QStringLiteral("surface_id")).toString();
    for (const QJsonValue &value : payload.value(QStringLiteral("menus")).toArray()) {
        const QJsonObject section = value.toObject();
        const bool fileRole = section.value(QStringLiteral("role")).toString() == QStringLiteral("file");
        QMenu *menu = m_fileMenu;
        if (!fileRole) {
            menu = new QMenu(this);
            insertMenu(m_helpMenu->menuAction(), menu);
            m_preparedMenus.append(menu);
        }
        menu->setTitle(section.value(QStringLiteral("label")).toString());
        for (const QJsonValue &itemValue : section.value(QStringLiteral("items")).toArray()) {
            const QJsonObject item = itemValue.toObject();
            auto *action = new QAction(item.value(QStringLiteral("label")).toString(), menu);
            action->setEnabled(item.value(QStringLiteral("enabled")).toBool());
            action->setToolTip(item.value(QStringLiteral("accessibility_label")).toString());
            const QString shortcut = item.value(QStringLiteral("shortcut")).toString();
            if (shortcut == QStringLiteral("import")) action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
            if (shortcut == QStringLiteral("undo")) action->setShortcut(QKeySequence::Undo);
            if (shortcut == QStringLiteral("back")) action->setShortcut(QKeySequence::Back);
            const QString interactionId = item.value(QStringLiteral("interaction_id")).toString();
            connect(action, &QAction::triggered, this, [this, surfaceId, interactionId]() {
                emit actionActivated(surfaceId, interactionId);
            });
            if (fileRole) menu->insertAction(m_fileSeparator, action);
            else menu->addAction(action);
            m_preparedActions.append(action);
        }
    }
}

VauchiMenuBar::VauchiMenuBar(QWidget *parent) : QMenuBar(parent) {
    // "&" prefix is a Qt keyboard accelerator hint, kept outside
    // the i18n value because accelerator assignment is platform-local.

    m_fileMenu = addMenu(QStringLiteral("&") + tr_vauchi("menu.file", "File"));
    m_fileSeparator = m_fileMenu->addSeparator();

    auto *quitAction = m_fileMenu->addAction(
        QStringLiteral("&")
        + tr_vauchi("help.quit", "Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);
    connect(quitAction, &QAction::triggered,
            this, &VauchiMenuBar::quitRequested);

    // Help menu
    m_helpMenu = addMenu(
        QStringLiteral("&")
        + tr_vauchi("nav.help", "Help"));
    QString aboutLabel =
        tr_vauchi("settings.about", "About")
        + QStringLiteral(" Vauchi");
    auto *aboutAction = m_helpMenu->addAction(
        QStringLiteral("&") + aboutLabel);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QString title =
            tr_vauchi("settings.about", "About")
            + QStringLiteral(" Vauchi");
        QMessageBox::about(this, title,
            tr_vauchi("about.body",
                      "Vauchi \u2014 Privacy-focused updatable "
                      "contact cards.\n\n"
                      "Version 0.5.0\n"
                      "License: AGPL-3.0-or-later (core), GPL-3.0-or-later (app)"));
    });
}
