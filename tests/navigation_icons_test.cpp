// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Tests for navigationIcon: the token-to-theme-icon table Core's
/// `icon_token` is resolved through before a navigation row is drawn.

#include "../src/coreui/navigationicons.h"
#include <QApplication>
#include <QImage>
#include <QPalette>
#include <QSet>
#include <QString>
#include <cassert>
#include <cstdio>

/// The icon tokens Core attaches to navigation items, mirroring
/// `tab_metadata` in `vauchi-app/src/ui/app_engine/navigation.rs`. Core owns
/// the list; this copy exists so the table can be proven total over
/// everything Core ships today.
static const char *const kCoreNavigationTokens[] = {
    "person.crop.rectangle",
    "person.2",
    "qrcode",
    "folder",
    "tag",
    "mappin.and.ellipse",
    "person.badge.plus",
    "gearshape",
    "questionmark.circle",
    "key.horizontal",
    "laptopcomputer",
    "externaldrive",
    "hand.raised",
    "bubble.left.and.bubble.right",
    "house",
    "list.bullet.rectangle",
};

/// The exchange pictograms Core names as `pictogram.exchange.<name>`,
/// mirroring `assets/pictograms/exchange/`. This copy exists so the bundle can
/// be proven to carry every one of them.
static const char *const kCoreExchangePictograms[] = {
    "glance", "hover", "bump", "shake", "magic",
    "tap_tap", "tap_hover_shake", "link", "cable",
};

// --- Test: each token names the theme icon it is supposed to name ---
static void test_tokens_map_to_their_theme_icons() {
    assert(vauchi::navigationIconNames("person.crop.rectangle").first()
           == QStringLiteral("user-identity"));
    assert(vauchi::navigationIconNames("person.2").first()
           == QStringLiteral("system-users"));
    assert(vauchi::navigationIconNames("qrcode").first()
           == QStringLiteral("view-barcode-qr"));
    assert(vauchi::navigationIconNames("folder").first()
           == QStringLiteral("folder"));
    assert(vauchi::navigationIconNames("tag").first()
           == QStringLiteral("tag"));
    assert(vauchi::navigationIconNames("mappin.and.ellipse").first()
           == QStringLiteral("mark-location"));
    assert(vauchi::navigationIconNames("person.badge.plus").first()
           == QStringLiteral("list-add-user"));
    assert(vauchi::navigationIconNames("gearshape").first()
           == QStringLiteral("configure"));
    assert(vauchi::navigationIconNames("questionmark.circle").first()
           == QStringLiteral("help-contents"));
    assert(vauchi::navigationIconNames("key.horizontal").first()
           == QStringLiteral("dialog-password"));
    assert(vauchi::navigationIconNames("laptopcomputer").first()
           == QStringLiteral("computer-laptop"));
    assert(vauchi::navigationIconNames("hand.raised").first()
           == QStringLiteral("security-high"));
    assert(vauchi::navigationIconNames("bubble.left.and.bubble.right").first()
           == QStringLiteral("dialog-messages"));
    assert(vauchi::navigationIconNames("list.bullet.rectangle").first()
           == QStringLiteral("view-list-details"));
    assert(vauchi::navigationIconNames("house").first()
           == QStringLiteral("go-home"));
    printf("  PASS: tokens_map_to_their_theme_icons\n");
}

// --- Test: backup is local storage, never a cloud ---
// Vauchi's backup is guardian-held and on-device; a cloud glyph would
// misrepresent the threat model to the one reader least able to check it.
static void test_backup_names_a_drive_not_a_cloud() {
    const QStringList names = vauchi::navigationIconNames("externaldrive");
    assert(names.first() == QStringLiteral("drive-harddisk"));
    for (const QString &name : names) {
        assert(!name.contains(QStringLiteral("cloud")));
        assert(!name.contains(QStringLiteral("network")));
    }
    printf("  PASS: backup_names_a_drive_not_a_cloud\n");
}

// --- Test: no two destinations share a first-choice icon ---
static void test_distinct_tokens_get_distinct_icons() {
    QSet<QString> seen;
    for (const char *const token : kCoreNavigationTokens) {
        const QString name = vauchi::navigationIconNames(token).first();
        assert(name != vauchi::navigationIconNames("no.such.token").first());
        assert(!seen.contains(name));
        seen.insert(name);
    }
    printf("  PASS: distinct_tokens_get_distinct_icons\n");
}

// --- Test: a token this build has not learned still names something ---
static void test_unknown_and_absent_tokens_fall_back() {
    const QStringList fallback =
        vauchi::navigationIconNames("sparkles.rectangle.not.a.token");
    assert(fallback.first() == QStringLiteral("applications-other"));
    assert(vauchi::navigationIconNames("") == fallback);
    assert(vauchi::navigationIconNames("   ") == fallback);
    printf("  PASS: unknown_and_absent_tokens_fall_back\n");
}

// --- Test: no candidate name is a thin "-symbolic" outline. Prefer
// filled Breeze names over outline ones where both exist for a concept —
// a "-symbolic" glyph loses definition at list-row size and is the first
// thing to disappear for a low-vision reader (see the table's own comment
// in navigationicons.cpp, which this test now enforces instead of leaving
// as an unchecked promise).
static void test_no_candidate_uses_symbolic_variants() {
    for (const char *const token : kCoreNavigationTokens) {
        for (const QString &name : vauchi::navigationIconNames(token)) {
            assert(!name.contains(QStringLiteral("-symbolic"))
                   && "a Breeze name reverted to a thin -symbolic outline");
        }
    }
    for (const QString &name :
         vauchi::navigationIconNames(QStringLiteral("no.such.token"))) {
        assert(!name.contains(QStringLiteral("-symbolic")));
    }
    printf("  PASS: no_candidate_uses_symbolic_variants\n");
}

// --- Test: every candidate list ends up resolvable to a drawable icon ---
// A themeless session (a minimal container, or a desktop with no icon theme
// installed) is the case where QIcon::fromTheme returns nothing for every
// name; the row must still get a glyph rather than an empty gap.
static void test_every_token_resolves_to_a_drawable_icon() {
    for (const char *const token : kCoreNavigationTokens) {
        assert(!vauchi::navigationIcon(token).isNull());
    }
    assert(!vauchi::navigationIcon("sparkles.rectangle.not.a.token").isNull());
    assert(!vauchi::navigationIcon("").isNull());
    printf("  PASS: every_token_resolves_to_a_drawable_icon\n");
}

// --- Test: a pictogram token names its bundled pictogram ---
static void test_pictogram_token_names_its_bundled_pictogram() {
    assert(vauchi::navigationIconNames("pictogram.exchange.hover").first()
           == QStringLiteral(":/pictograms/exchange/hover.svg"));
    assert(vauchi::navigationIconNames("pictogram.exchange.tap_hover_shake")
               .first()
           == QStringLiteral(":/pictograms/exchange/tap_hover_shake.svg"));
    printf("  PASS: pictogram_token_names_its_bundled_pictogram\n");
}

// --- Test: every exchange pictogram is drawn in the palette's text colour ---
// The pictogram must follow the foreground colour so it stays legible on
// every theme, rather than keeping whatever colour the SVG was authored in.
static void test_every_exchange_pictogram_draws_in_the_text_color() {
    const QPalette original = QApplication::palette();
    QPalette palette = original;
    const QColor text(200, 30, 60);
    palette.setColor(QPalette::WindowText, text);
    QApplication::setPalette(palette);

    for (const char *const name : kCoreExchangePictograms) {
        const QString token = QStringLiteral("pictogram.exchange.%1")
                                  .arg(QLatin1String(name));
        const QImage image = vauchi::navigationIcon(token)
                                 .pixmap(48, 48)
                                 .toImage()
                                 .convertToFormat(QImage::Format_ARGB32);
        int opaque = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                if (pixel.alpha() < 128) {
                    continue;
                }
                ++opaque;
                assert(qAbs(pixel.red() - text.red()) <= 4
                       && qAbs(pixel.green() - text.green()) <= 4
                       && qAbs(pixel.blue() - text.blue()) <= 4
                       && "pictogram not drawn in the palette text colour");
            }
        }
        assert(opaque > 50 && "pictogram drew nothing");
    }

    QApplication::setPalette(original);
    printf("  PASS: every_exchange_pictogram_draws_in_the_text_color\n");
}

// --- Test: a pictogram this build does not bundle falls back as today ---
// Core may name a pictogram before this build carries it.
static void test_missing_pictogram_falls_back() {
    const QImage missing =
        vauchi::navigationIcon("pictogram.exchange.not_bundled")
            .pixmap(32, 32)
            .toImage();
    const QImage fallback =
        vauchi::navigationIcon("no.such.token").pixmap(32, 32).toImage();
    assert(!missing.isNull());
    assert(missing == fallback);
    printf("  PASS: missing_pictogram_falls_back\n");
}

// --- Test: a malformed pictogram token is just an unknown token ---
static void test_malformed_pictogram_tokens_fall_back() {
    const QStringList fallback = vauchi::navigationIconNames("no.such.token");
    for (const char *const token : {
             "pictogram",
             "pictogram.",
             "pictogram.exchange",
             "pictogram.exchange.",
             "pictogram..hover",
             "pictogram.exchange.hover.extra",
             "pictogram.exchange.../hover",
             "pictogram.exchange.Hover",
             "pictogram.exchange.ho ver",
         }) {
        assert(vauchi::navigationIconNames(QString::fromLatin1(token))
               == fallback);
    }
    printf("  PASS: malformed_pictogram_tokens_fall_back\n");
}

// --- Test: list rows and status nodes get an icon only for a pictogram ---
// Those surfaces drew no icon before pictograms existed, so every other
// token must leave them as they were rather than add a fallback marker.
static void test_content_pictogram_only_for_bundled_pictograms() {
    const QPalette original = QApplication::palette();
    QPalette palette = original;
    const QColor text(20, 160, 90);
    palette.setColor(QPalette::WindowText, text);
    QApplication::setPalette(palette);

    const QIcon hover = vauchi::contentPictogram("pictogram.exchange.hover");
    assert(!hover.isNull());
    const QImage image =
        hover.pixmap(48, 48).toImage().convertToFormat(QImage::Format_ARGB32);
    int opaque = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() >= 128) {
                ++opaque;
                assert(qAbs(pixel.green() - text.green()) <= 4);
            }
        }
    }
    assert(opaque > 50);
    QApplication::setPalette(original);

    for (const char *const token : kCoreNavigationTokens) {
        assert(vauchi::contentPictogram(QString::fromLatin1(token)).isNull());
    }
    assert(vauchi::contentPictogram("").isNull());
    assert(vauchi::contentPictogram("no.such.token").isNull());
    assert(vauchi::contentPictogram("pictogram.exchange.not_bundled").isNull());
    printf("  PASS: content_pictogram_only_for_bundled_pictograms\n");
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    printf("navigation_icons_test\n");
    test_tokens_map_to_their_theme_icons();
    test_backup_names_a_drive_not_a_cloud();
    test_distinct_tokens_get_distinct_icons();
    test_unknown_and_absent_tokens_fall_back();
    test_no_candidate_uses_symbolic_variants();
    test_every_token_resolves_to_a_drawable_icon();
    test_pictogram_token_names_its_bundled_pictogram();
    test_every_exchange_pictogram_draws_in_the_text_color();
    test_missing_pictogram_falls_back();
    test_malformed_pictogram_tokens_fall_back();
    test_content_pictogram_only_for_bundled_pictograms();
    printf("ALL TESTS PASSED\n");
    return 0;
}
