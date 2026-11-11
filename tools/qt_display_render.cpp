// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// Offscreen renders on the display library the apps assemble, for VRT and
// test:screen-catalog (private#1029). Same arguments and output names as
// the legacy `qvauchi --render-catalog` / `--render-fixture`, which this
// replaces once the legacy app is retired.
//
//   qt_display_render --render-catalog <catalog.json> <out-dir> [w] [h]
//   qt_display_render --render-fixture <surface.json> <out.png> [w] [h]

#include "coreui/displayhost.h"
#include "coreui/presentationsurface.h"
#include "coreui/screencatalog.h"
#include "coreui/thememanager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <cstdio>

// rcc's initializer has C++ linkage and lives outside any namespace.
static void initializeBundledResources() {
    Q_INIT_RESOURCE(resources);
}

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

int sizeArg(const QStringList &args, int index, int fallback) {
    const int value = args.value(index).toInt();
    return value > 0 ? value : fallback;
}

// Widgets read the palette and stylesheet when they are built, so each
// variant applies theme and font before its first screen.
void applyVariant(const Variant &variant) {
    if (variant.light) {
        ThemeManager::applyDefaultLightTheme();
    } else {
        ThemeManager::applyDefaultTheme();
    }
    QFont font = ThemeManager::uiFont();
    if (variant.largeText) {
        qreal pointSize = font.pointSizeF();
        if (pointSize <= 0) pointSize = QApplication::font().pointSizeF();
        if (pointSize <= 0) pointSize = 12;
        font.setPointSizeF(pointSize * kLargeTextScale);
    }
    QApplication::setFont(font);
}

// The harness grabs without entering the event loop: flush deferred
// deletes and pending layout before the grab.
void settle() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

bool save(const QPixmap &pixmap, const QString &path, const char *mode) {
    const bool ok = pixmap.save(path);
    std::fprintf(stderr, "[%s] %s %s (%dx%d)\n", mode, ok ? "wrote" : "FAILED",
                 qUtf8Printable(path), pixmap.width(), pixmap.height());
    return ok;
}

void ignoreEvent(const char *, void *) {}

bool renderScreen(const vauchi::ScreenCatalogEntry &entry, const QString &outPath,
                  int width, int height) {
    QtDisplayHost host(ignoreEvent, nullptr);
    host.setWindowTitle(entry.title);
    host.resize(width, height);
    host.show();
    host.render(entry.commands);
    settle();
    return save(host.grab(), outPath, "render-catalog");
}

int renderCatalog(const QStringList &args, int index) {
    const QString jsonPath = args.value(index + 1);
    const QString outDir = args.value(index + 2);
    if (jsonPath.isEmpty() || outDir.isEmpty()) {
        std::fprintf(stderr, "usage: qt_display_render --render-catalog <catalog.json> "
                             "<out-dir> [w] [h]\n");
        return 2;
    }
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "cannot open %s\n", qUtf8Printable(jsonPath));
        return 2;
    }
    const vauchi::ScreenCatalog catalog = vauchi::parseScreenCatalog(file.readAll());
    if (!catalog.ok()) {
        std::fprintf(stderr, "[render-catalog] rejected %s: %s\n", qUtf8Printable(jsonPath),
                     qUtf8Printable(catalog.error));
        return 2;
    }
    if (!QDir().mkpath(outDir)) {
        std::fprintf(stderr, "cannot create %s\n", qUtf8Printable(outDir));
        return 2;
    }
    const QDir out(outDir);
    const int width = sizeArg(args, index + 3, kDefaultWidth);
    const int height = sizeArg(args, index + 4, kDefaultHeight);
    int failures = 0;
    for (const auto &variant : kVariants) {
        applyVariant(variant);
        for (const auto &entry : catalog.screens) {
            const QString path =
                out.filePath(vauchi::screenCatalogFileName(entry.codeId, variant.suffix));
            if (!renderScreen(entry, path, width, height)) ++failures;
        }
    }
    std::fprintf(stderr, "[render-catalog] %lld screens x %zu variants, %d failed\n",
                 static_cast<long long>(catalog.screens.size()),
                 sizeof(kVariants) / sizeof(kVariants[0]), failures);
    return failures == 0 ? 0 : 1;
}

int renderFixture(const QStringList &args, int index) {
    const QString jsonPath = args.value(index + 1);
    const QString outPath = args.value(index + 2);
    if (jsonPath.isEmpty() || outPath.isEmpty()) {
        std::fprintf(stderr,
                     "usage: qt_display_render --render-fixture <json> <out.png> [w] [h]\n");
        return 2;
    }
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "cannot open %s\n", qUtf8Printable(jsonPath));
        return 2;
    }
    const QJsonObject surface = QJsonDocument::fromJson(file.readAll()).object();
    ThemeManager::applyDefaultTheme();
    PresentationSurface renderer(surface);
    renderer.resize(sizeArg(args, index + 3, kDefaultWidth),
                    sizeArg(args, index + 4, kDefaultHeight));
    settle();
    return save(renderer.grab(), outPath, "render-fixture") ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    initializeBundledResources();
    ThemeManager::registerBrandFonts();
    app.setFont(ThemeManager::uiFont());
    const QStringList args = app.arguments();
    if (const int index = args.indexOf(QStringLiteral("--render-catalog")); index >= 0)
        return renderCatalog(args, index);
    if (const int index = args.indexOf(QStringLiteral("--render-fixture")); index >= 0)
        return renderFixture(args, index);
    std::fprintf(stderr, "usage: qt_display_render --render-catalog|--render-fixture ...\n");
    return 2;
}
