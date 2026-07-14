// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Vauchi — native Linux desktop app (Qt6).

#include "app.h"
#include "coreui/screenrenderer.h"
#include "coreui/thememanager.h"
#include "platform/screencaptureprotection.h"
#include "vauchi.h"
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <cstdio>

// `--render-fixture <screen.json> <out.png> [width] [height]` renders one
// ScreenModel JSON fixture through the production ScreenRenderer and saves a
// PNG, for the design screenshot catalog
// (problem 2026-06-12-device-screenshot-catalog). Runs headless under the Qt
// `offscreen` platform — set QT_QPA_PLATFORM=offscreen. Returns the process
// exit code; a negative result means "not in render-fixture mode".
static int maybeRenderFixture(const QStringList &args) {
    const int idx = args.indexOf(QStringLiteral("--render-fixture"));
    if (idx < 0) return -1;

    const QString jsonPath = args.value(idx + 1);
    const QString outPath = args.value(idx + 2);
    const int width = args.value(idx + 3, QStringLiteral("900")).toInt();
    const int height = args.value(idx + 4, QStringLiteral("1400")).toInt();
    if (jsonPath.isEmpty() || outPath.isEmpty()) {
        std::fprintf(stderr,
                     "usage: qvauchi --render-fixture <json> <out.png> [w] [h]\n");
        return 2;
    }

    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "cannot open %s\n", qUtf8Printable(jsonPath));
        return 2;
    }
    const QJsonObject screen = QJsonDocument::fromJson(f.readAll()).object();

    ThemeManager::applyDefaultTheme();
    // Null app selects ScreenRenderer's inert render-only mode: fixture
    // screenshots must not start persistence, network, audio, BLE, or NFC.
    auto *renderer = new ScreenRenderer(nullptr, nullptr);
    renderer->renderFixture(screen);
    renderer->resize(width > 0 ? width : 900, height > 0 ? height : 1400);
    // Component replacement uses deleteLater(); flush deferred deletes now,
    // since this harness grabs synchronously without running the event loop.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    const QPixmap pixmap = renderer->grab();
    const bool ok = pixmap.save(outPath);
    std::fprintf(stderr, "[render-fixture] %s %s (%dx%d)\n",
                 ok ? "wrote" : "FAILED", qUtf8Printable(outPath),
                 pixmap.width(), pixmap.height());
    delete renderer;
    return ok ? 0 : 1;
}

int main(int argc, char *argv[]) {
    QApplication qtApp(argc, argv);
    // Pin a deterministic UI font before any widget is constructed —
    // qvauchi sets no font-family, so otherwise every label inherits
    // Qt's host default (a monospace coding font on some dev boxes).
    qtApp.setFont(ThemeManager::uiFont());

    // Catalog capture mode exits before screen-capture protection, which
    // would otherwise blank the grabbed pixmap.
    const int fixtureRc = maybeRenderFixture(qtApp.arguments());
    if (fixtureRc >= 0) return fixtureRc;

    enableScreenCaptureProtection();
    qtApp.setApplicationName("vauchi");
    qtApp.setApplicationVersion("0.5.0");
    qtApp.setOrganizationName("vauchi");
    qtApp.setWindowIcon(QIcon(QStringLiteral(":/icons/vauchi.svg")));

    VauchiWindow window;
    window.show();

    return qtApp.exec();
}
