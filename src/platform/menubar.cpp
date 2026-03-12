// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menubar.h"
#include <QAction>
#include <QMessageBox>

VauchiMenuBar::VauchiMenuBar(QWidget *parent) : QMenuBar(parent) {
    // File menu
    auto *fileMenu = addMenu(tr("&File"));
    auto *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &VauchiMenuBar::quitRequested);

    // Help menu
    auto *helpMenu = addMenu(tr("&Help"));
    auto *aboutAction = helpMenu->addAction(tr("&About Vauchi"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About Vauchi"),
            tr("Vauchi — Privacy-focused updatable contact cards.\n\n"
               "Version 0.5.0\n"
               "License: GPL-3.0-or-later"));
    });
}
