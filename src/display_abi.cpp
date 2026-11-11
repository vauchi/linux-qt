// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The three-call C ABI of the Qt display shell (include/qt_display.h).
// The host lives on the GUI thread; command batches cross by queued delivery.
#include "qt_display.h"

#include "coreui/displayhost.h"
#include "coreui/thememanager.h"
#include "platform/deeplink.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QMainWindow>
#include <QMetaObject>
#include <QPointer>
#include <QScrollArea>
#include <QTimer>

#include <atomic>
#include <cstdio>
#include <memory>

// rcc's initializer has C++ linkage, so it cannot be declared in the C ABI body.
static void initializeBundledResources() {
    Q_INIT_RESOURCE(resources);
}

namespace {

QPointer<QtDisplayHost> g_host;
std::atomic<bool> g_running{false};

}  // namespace

extern "C" int qt_display_start(int argc, char **argv,
                                QtDisplayEventCallback on_event, void *user_data) {
    return qt_display_start_with_poll(argc, argv, on_event, nullptr, user_data);
}

extern "C" int qt_display_start_with_poll(int argc, char **argv,
                                          QtDisplayEventCallback on_event,
                                          QtDisplayPollCallback on_poll, void *user_data) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("vauchi"));
    QCoreApplication::setOrganizationName(QStringLiteral("vauchi"));
    QGuiApplication::setDesktopFileName(QStringLiteral("app.vauchi.Vauchi"));
    initializeBundledResources();
    ThemeManager::registerBrandFonts();
    app.setFont(ThemeManager::uiFont());
    ThemeManager::applyDefaultTheme();
    QtDisplayHost host(on_event, user_data);
    g_host = &host;
    host.show();
    g_running = true;
    host.reportEnvironment();
    host.reportCapabilities();
    vauchi::DeepLinkRelay relay(vauchi::DeepLinkRelay::defaultServerName());
    QObject::connect(&relay, &vauchi::DeepLinkRelay::linkReceived, &host,
                     [&host](const QString &uri) {
                         host.reportDeepLink(uri);
                         host.raise();
                         host.activateWindow();
                     });
    if (!relay.listen())
        std::fprintf(stderr, "[vauchi] deep links from later launches unavailable\n");
    for (const QString &uri : vauchi::deepLinkUris(app.arguments())) host.reportDeepLink(uri);
    QTimer pollTimer;
    if (on_poll) {
        QObject::connect(&pollTimer, &QTimer::timeout, &host, [on_poll, user_data] {
            on_poll(user_data);
        });
        pollTimer.start(50);
        QTimer::singleShot(0, &host, [on_poll, user_data] { on_poll(user_data); });
    }
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

extern "C" int qt_display_hand_off_links(int argc, char **argv) {
    QStringList args;
    for (int i = 0; i < argc; ++i) args.append(QString::fromLocal8Bit(argv[i]));
    const QStringList links = vauchi::deepLinkUris(args);
    if (links.isEmpty()) return 0;
    // The app calls this before its engine and before Qt runs; the local
    // socket needs an application object for the duration of the call.
    int count = argc;
    std::unique_ptr<QCoreApplication> app;
    if (!QCoreApplication::instance()) app = std::make_unique<QCoreApplication>(count, argv);
    vauchi::DeepLinkRelay relay(vauchi::DeepLinkRelay::defaultServerName());
    return relay.handOff(links) ? 1 : 0;
}
