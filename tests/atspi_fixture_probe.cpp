// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

//! Render a `SurfaceSpec` fixture through the production `PresentationSurface`
//! and keep the window on the accessibility bus so an external AT-SPI reader
//! can walk the live tree.
//!
//! Counterpart of linux-gtk's `render_fixture --keep-open`. Widget-level
//! assertions cannot see the defect this exists for: a toolkit that reports
//! its own visible text in place of Core's accessible name still satisfies
//! `accessibleName()`, and only the bus shows which one an assistive
//! technology receives. See
//! `problems/2026-08-21-linux-shells-drop-core-a11y`.
//!
//! Usage: atspi_fixture_probe <fixture.json>

#include "coreui/presentationsurface.h"

#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMainWindow>

#include <cstdio>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: atspi_fixture_probe <fixture.json>\n");
        return 2;
    }

    QFile file(QString::fromLocal8Bit(argv[1]));
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "cannot read fixture %s\n", argv[1]);
        return 2;
    }
    QJsonParseError parseError{};
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        std::fprintf(stderr, "cannot decode fixture %s: %s\n", argv[1],
                     qPrintable(parseError.errorString()));
        return 2;
    }

    const QJsonObject surface = document.object();
    auto *window = new QMainWindow;
    window->setWindowTitle(surface.value(QStringLiteral("title")).toString());
    window->setCentralWidget(new PresentationSurface(surface));
    window->resize(900, 1400);
    window->show();
    std::fprintf(stderr, "[atspi-fixture-probe] presented %s\n", argv[1]);
    return app.exec();
}
