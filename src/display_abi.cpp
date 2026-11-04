// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The three-call C ABI of the Qt display shell (include/qt_display.h).
// The host lives on the GUI thread; command batches cross by queued delivery.
#include "qt_display.h"

#include "coreui/displayhost.h"
#include "coreui/thememanager.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QMainWindow>
#include <QMetaObject>
#include <QPointer>
#include <QScrollArea>

#include <atomic>

namespace {

QPointer<QtDisplayHost> g_host;
std::atomic<bool> g_running{false};

}  // namespace

extern "C" int qt_display_start(int argc, char **argv,
                                QtDisplayEventCallback on_event, void *user_data) {
    QApplication app(argc, argv);
    ThemeManager::registerBrandFonts();
    app.setFont(ThemeManager::uiFont());
    ThemeManager::applyDefaultTheme();
    QtDisplayHost host(on_event, user_data);
    g_host = &host;
    host.show();
    g_running = true;
    const int rc = app.exec();
    g_running = false;
    g_host = nullptr;
    return rc;
}

extern "C" int qt_display_render(const char *commands_json, size_t len) {
    if (!g_running || !g_host) return -1;
    const QJsonDocument doc =
        QJsonDocument::fromJson(QByteArray(commands_json, static_cast<int>(len)));
    if (!doc.isArray()) return -1;
    const QJsonArray commands = doc.array();
    QMetaObject::invokeMethod(
        g_host, [commands] { if (g_host) g_host->render(commands); },
        Qt::QueuedConnection);
    return 0;
}

extern "C" void qt_display_stop(void) {
    if (!g_running) return;
    QMetaObject::invokeMethod(qApp, [] { qApp->quit(); }, Qt::QueuedConnection);
}
