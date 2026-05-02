// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Vauchi — native Linux desktop app (Qt6).

#include "app.h"
#include "platform/screencaptureprotection.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication qtApp(argc, argv);
    enableScreenCaptureProtection();
    qtApp.setApplicationName("vauchi");
    qtApp.setApplicationVersion("0.5.0");
    qtApp.setOrganizationName("vauchi");
    qtApp.setWindowIcon(QIcon(QStringLiteral(":/icons/vauchi.svg")));

    VauchiWindow window;
    window.show();

    return qtApp.exec();
}
