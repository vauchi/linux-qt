// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rendercatalog.h"

#include "coreui/presentationcontroller.h"
#include "coreui/screencatalog.h"
#include "coreui/thememanager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QPixmap>
#include <cstdio>

namespace {

constexpr int kDefaultWidth = 900;
constexpr int kDefaultHeight = 1400;
constexpr qreal kLargeTextScale = 1.5;

struct Variant {
    QString suffix;
    bool light;
    bool largeText;
};

const Variant kVariants[] = {
    {QString(), false, false},
    {QStringLiteral("light"), true, false},
    {QStringLiteral("large"), false, true},
};

// Theme and font are application-global, so every variant re-applies both
// before constructing its widgets: PresentationSurface reads the palette
// and stylesheet at construction time.
void applyVariant(const Variant &variant) {
    if (variant.light) {
        ThemeManager::applyDefaultLightTheme();
    } else {
        ThemeManager::applyDefaultTheme();
    }
    QFont font = ThemeManager::uiFont();
    if (variant.largeText) {
        qreal pointSize = font.pointSizeF();
        if (pointSize <= 0) {
            pointSize = QApplication::font().pointSizeF();
        }
        if (pointSize <= 0) {
            pointSize = 12;
        }
        font.setPointSizeF(pointSize * kLargeTextScale);
    }
    QApplication::setFont(font);
}

// Mirrors VauchiWindow's layout (a QMainWindow whose central widget hosts the
// controller in a horizontal box) minus the pieces that need a live Core
// app: menu bar, tray, and event callbacks.
bool renderScreen(const vauchi::ScreenCatalogEntry &entry,
                  const QString &outPath, int width, int height) {
    QMainWindow window;
    window.setWindowTitle(entry.title);
    auto *central = new QWidget(&window);
    auto *layout = new QHBoxLayout(central);
    auto *controller = new PresentationController(nullptr, central);
    layout->addWidget(controller, 1);
    window.setCentralWidget(central);
    window.resize(width, height);
    window.show();
    controller->dispatchCommands(entry.commands);
    // PresentationSurface teardown uses deleteLater(); flush deferred deletes
    // and pending layout events before grabbing, since this harness never
    // enters the event loop.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    const QPixmap pixmap = window.grab();
    const bool ok = pixmap.save(outPath);
    std::fprintf(stderr, "[render-catalog] %s %s (%dx%d)\n",
                 ok ? "wrote" : "FAILED", qUtf8Printable(outPath),
                 pixmap.width(), pixmap.height());
    return ok;
}

} // namespace

int maybeRenderCatalog(const QStringList &args) {
    const int idx = args.indexOf(QStringLiteral("--render-catalog"));
    if (idx < 0) return -1;

    const QString jsonPath = args.value(idx + 1);
    const QString outDir = args.value(idx + 2);
    const int width = args.value(idx + 3, QString::number(kDefaultWidth)).toInt();
    const int height =
        args.value(idx + 4, QString::number(kDefaultHeight)).toInt();
    if (jsonPath.isEmpty() || outDir.isEmpty()) {
        std::fprintf(stderr,
                     "usage: qvauchi --render-catalog <catalog.json> <out-dir> "
                     "[w] [h]\n");
        return 2;
    }

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "cannot open %s\n", qUtf8Printable(jsonPath));
        return 2;
    }
    const vauchi::ScreenCatalog catalog =
        vauchi::parseScreenCatalog(file.readAll());
    if (!catalog.ok()) {
        std::fprintf(stderr, "[render-catalog] rejected %s: %s\n",
                     qUtf8Printable(jsonPath), qUtf8Printable(catalog.error));
        return 2;
    }
    if (!QDir().mkpath(outDir)) {
        std::fprintf(stderr, "cannot create %s\n", qUtf8Printable(outDir));
        return 2;
    }

    const QDir out(outDir);
    int failures = 0;
    for (const auto &variant : kVariants) {
        applyVariant(variant);
        for (const auto &entry : catalog.screens) {
            const QString outPath = out.filePath(
                vauchi::screenCatalogFileName(entry.codeId, variant.suffix));
            if (!renderScreen(entry, outPath, width > 0 ? width : kDefaultWidth,
                              height > 0 ? height : kDefaultHeight)) {
                ++failures;
            }
        }
    }
    std::fprintf(stderr, "[render-catalog] %lld screens x %zu variants, %d failed\n",
                 static_cast<long long>(catalog.screens.size()),
                 sizeof(kVariants) / sizeof(kVariants[0]), failures);
    return failures == 0 ? 0 : 1;
}
