// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menubar.h"
#include "../i18n.h"
#include <QAction>
#include <QMessageBox>

VauchiMenuBar::VauchiMenuBar(QWidget *parent) : QMenuBar(parent) {
    // "&" prefix is a Qt keyboard accelerator hint, kept outside
    // the i18n value because accelerator assignment is platform-local.

    // File menu (no i18n key — desktop convention, not app content)
    auto *fileMenu = addMenu(QStringLiteral("&File"));
    auto *quitAction = fileMenu->addAction(
        QStringLiteral("&")
        + tr_vauchi("help.quit", QStringLiteral("Quit")));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered,
            this, &VauchiMenuBar::quitRequested);

    // Help menu
    auto *helpMenu = addMenu(
        QStringLiteral("&")
        + tr_vauchi("nav.help", QStringLiteral("Help")));
    QString aboutLabel =
        tr_vauchi("settings.about", QStringLiteral("About"))
        + QStringLiteral(" Vauchi");
    auto *aboutAction = helpMenu->addAction(
        QStringLiteral("&") + aboutLabel);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QString title =
            tr_vauchi("settings.about", QStringLiteral("About"))
            + QStringLiteral(" Vauchi");
        QMessageBox::about(this, title,
            QStringLiteral(
                "Vauchi \u2014 Privacy-focused updatable "
                "contact cards.\n\n"
                "Version 0.5.0\n"
                "License: GPL-3.0-or-later"));
    });
}
