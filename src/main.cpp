// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Vauchi — native Linux desktop app (Qt6).

#include "app.h"
#include "coreui/thememanager.h"
#include "platform/screencaptureprotection.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication qtApp(argc, argv);
    // Pin a deterministic UI font before any widget is constructed —
    // qvauchi sets no font-family, so otherwise every label inherits
    // Qt's host default (a monospace coding font on some dev boxes).
    qtApp.setFont(ThemeManager::uiFont());
    enableScreenCaptureProtection();
    qtApp.setApplicationName("vauchi");
    qtApp.setApplicationVersion("0.5.0");
    qtApp.setOrganizationName("vauchi");
    qtApp.setWindowIcon(QIcon(QStringLiteral(":/icons/vauchi.svg")));

    VauchiWindow window;
    window.show();

    return qtApp.exec();
}
