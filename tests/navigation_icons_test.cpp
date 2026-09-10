// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Tests for navigationIcon: the token-to-theme-icon table Core's
/// `icon_token` is resolved through before a navigation row is drawn.

#include "../src/coreui/navigationicons.h"
#include <QApplication>
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

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    printf("navigation_icons_test\n");
    test_tokens_map_to_their_theme_icons();
    test_backup_names_a_drive_not_a_cloud();
    test_distinct_tokens_get_distinct_icons();
    test_unknown_and_absent_tokens_fall_back();
    test_no_candidate_uses_symbolic_variants();
    test_every_token_resolves_to_a_drawable_icon();
    printf("ALL TESTS PASSED\n");
    return 0;
}
