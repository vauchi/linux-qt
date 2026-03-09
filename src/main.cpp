// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Vauchi — native Linux desktop app (Qt6).

#include "app.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication qtApp(argc, argv);
    qtApp.setApplicationName("Vauchi");
    qtApp.setApplicationVersion("0.5.0");
    qtApp.setOrganizationName("vauchi");

    VauchiWindow window;
    window.show();

    return qtApp.exec();
}
