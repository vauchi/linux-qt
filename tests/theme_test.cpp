// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Tests for ThemeManager: palette generation, stylesheet output, file loading.

#include "../src/coreui/thememanager.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPalette>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// --- Test: default colors match core's Catppuccin Mocha ---
static void test_default_colors() {
    QJsonObject colors = ThemeManager::defaultColors();
    assert(colors["bg-primary"].toString() == "#1e1e2e");
    assert(colors["bg-secondary"].toString() == "#181825");
    assert(colors["bg-tertiary"].toString() == "#313244");
    assert(colors["text-primary"].toString() == "#cdd6f4");
    assert(colors["text-secondary"].toString() == "#a6adc8");
    assert(colors["accent"].toString() == "#89b4fa");
    assert(colors["accent-dark"].toString() == "#74c7ec");
    assert(colors["success"].toString() == "#a6e3a1");
    assert(colors["error"].toString() == "#f38ba8");
    assert(colors["warning"].toString() == "#fab387");
    assert(colors["border"].toString() == "#45475a");
    printf("  PASS: default_colors\n");
}

// --- Test: palette generation maps colors correctly ---
static void test_palette_from_colors() {
    QJsonObject colors = ThemeManager::defaultColors();
    QPalette palette = ThemeManager::paletteFromColors(colors);

    // Window background should be bg-primary
    assert(palette.color(QPalette::Window) == QColor("#1e1e2e"));
    // Window text should be text-primary
    assert(palette.color(QPalette::WindowText) == QColor("#cdd6f4"));
    // Base should be bg-secondary
    assert(palette.color(QPalette::Base) == QColor("#181825"));
    // Highlight should be accent
    assert(palette.color(QPalette::Highlight) == QColor("#89b4fa"));
    // Placeholder text should be text-secondary
    assert(palette.color(QPalette::PlaceholderText) == QColor("#a6adc8"));
    printf("  PASS: palette_from_colors\n");
}

// --- Test: different colors produce different palettes ---
static void test_different_themes_different_palettes() {
    QJsonObject dark;
    dark["bg-primary"] = "#000000";
    dark["bg-secondary"] = "#111111";
    dark["bg-tertiary"] = "#222222";
    dark["text-primary"] = "#ffffff";
    dark["text-secondary"] = "#cccccc";
    dark["accent"] = "#0000ff";
    dark["accent-dark"] = "#000099";
    dark["success"] = "#00ff00";
    dark["error"] = "#ff0000";
    dark["warning"] = "#ffff00";
    dark["border"] = "#333333";

    QJsonObject light;
    light["bg-primary"] = "#ffffff";
    light["bg-secondary"] = "#eeeeee";
    light["bg-tertiary"] = "#dddddd";
    light["text-primary"] = "#000000";
    light["text-secondary"] = "#333333";
    light["accent"] = "#0066cc";
    light["accent-dark"] = "#004488";
    light["success"] = "#228b22";
    light["error"] = "#cc0000";
    light["warning"] = "#cc8800";
    light["border"] = "#cccccc";

    QPalette darkPal = ThemeManager::paletteFromColors(dark);
    QPalette lightPal = ThemeManager::paletteFromColors(light);

    assert(darkPal.color(QPalette::Window) != lightPal.color(QPalette::Window));
    assert(darkPal.color(QPalette::WindowText) != lightPal.color(QPalette::WindowText));
    printf("  PASS: different_themes_different_palettes\n");
}

// --- Test: stylesheet generation includes theme colors ---
static void test_stylesheet_from_colors() {
    QJsonObject colors = ThemeManager::defaultColors();
    QString stylesheet = ThemeManager::stylesheetFromColors(colors);

    assert(stylesheet.contains("#1e1e2e"));  // bg-primary
    assert(stylesheet.contains("#cdd6f4"));  // text-primary
    assert(stylesheet.contains("#181825"));  // bg-secondary
    assert(stylesheet.contains("#45475a"));  // border
    assert(stylesheet.contains("QMainWindow"));
    assert(stylesheet.contains("QListWidget#sidebar"));
    printf("  PASS: stylesheet_from_colors\n");
}

// --- Test: load from file with valid JSON ---
static void test_load_from_file_valid() {
    auto dir = fs::temp_directory_path() / "vauchi-theme-test";
    fs::create_directories(dir);
    auto path = dir / "themes.json";

    QJsonArray themes;
    QJsonObject theme;
    theme["id"] = "test";
    theme["name"] = "Test";
    theme["colors"] = ThemeManager::defaultColors();
    themes.append(theme);

    QJsonDocument doc(themes);
    std::ofstream out(path.string());
    out << doc.toJson().toStdString();
    out.close();

    bool loaded = ThemeManager::loadFromFile(QString::fromStdString(path.string()));
    assert(loaded);

    fs::remove_all(dir);
    printf("  PASS: load_from_file_valid\n");
}

// --- Test: load from nonexistent file returns false ---
static void test_load_from_file_missing() {
    bool loaded = ThemeManager::loadFromFile("/tmp/nonexistent-vauchi-themes.json");
    assert(!loaded);
    printf("  PASS: load_from_file_missing\n");
}

// --- Test: empty colors object is handled gracefully ---
static void test_empty_colors() {
    QJsonObject empty;
    QPalette palette = ThemeManager::paletteFromColors(empty);
    // Should not crash — colors default to invalid/black
    (void)palette;
    printf("  PASS: empty_colors\n");
}

// --- Test: currentColors() defaults to Catppuccin Mocha before applyTheme() ---
static void test_current_colors_defaults() {
    // NOTE: must run before test_current_colors_updates_after_apply
    QJsonObject current = ThemeManager::currentColors();
    assert(current["bg-primary"].toString() == "#1e1e2e");
    assert(current["accent"].toString() == "#89b4fa");
    assert(current["error"].toString() == "#f38ba8");
    printf("  PASS: current_colors_defaults\n");
}

// --- Test: styleForRole returns CSS containing theme color for each role ---
static void test_style_for_role_uses_default_palette() {
    // Default palette — Catppuccin Mocha
    QString primary = ThemeManager::styleForRole(ThemeRole::PrimaryButton);
    assert(primary.contains("#89b4fa"));      // accent
    assert(primary.contains("#cdd6f4"));      // text-primary
    // Radius + padding come from design tokens (Tokens::BorderRadius::SM,
    // Tokens::Spacing::SM, Tokens::Spacing::MD) — not hardcoded in the helper.
    assert(primary.contains("border-radius: 4px"));
    assert(primary.contains("padding: 8px 16px"));

    QString destructive = ThemeManager::styleForRole(ThemeRole::DestructiveButton);
    assert(destructive.contains("#f38ba8"));  // error
    assert(destructive.contains("#cdd6f4"));  // text-primary
    // PrimaryButton and DestructiveButton must diverge on fill color
    assert(primary != destructive);

    assert(ThemeManager::styleForRole(ThemeRole::StatusSuccess).contains("#a6e3a1"));
    assert(ThemeManager::styleForRole(ThemeRole::StatusError).contains("#f38ba8"));
    assert(ThemeManager::styleForRole(ThemeRole::StatusWarning).contains("#fab387"));
    assert(ThemeManager::styleForRole(ThemeRole::StatusInProgress).contains("#89b4fa"));
    assert(ThemeManager::styleForRole(ThemeRole::StatusNeutral).contains("#a6adc8"));

    assert(ThemeManager::styleForRole(ThemeRole::DestructiveText).contains("#f38ba8"));
    assert(ThemeManager::styleForRole(ThemeRole::SecondaryText).contains("#a6adc8"));
    assert(ThemeManager::styleForRole(ThemeRole::BannerInfo).contains("#313244"));
    assert(ThemeManager::styleForRole(ThemeRole::ErrorBorder).contains("#f38ba8"));

    // Regression guard: no role may return an empty stylesheet (switch coverage).
    ThemeRole all[] = {
        ThemeRole::PrimaryButton, ThemeRole::DestructiveButton,
        ThemeRole::StatusSuccess, ThemeRole::StatusError,
        ThemeRole::StatusWarning, ThemeRole::StatusInProgress,
        ThemeRole::StatusNeutral, ThemeRole::DestructiveText,
        ThemeRole::SecondaryText, ThemeRole::BannerInfo,
        ThemeRole::ErrorBorder,
    };
    for (ThemeRole r : all) {
        assert(!ThemeManager::styleForRole(r).isEmpty());
    }
    printf("  PASS: style_for_role_uses_default_palette\n");
}

// --- Test: applyTheme updates currentColors and styleForRole reflects it ---
static void test_style_for_role_updates_after_apply() {
    QJsonObject light;
    light["bg-primary"] = "#ffffff";
    light["bg-secondary"] = "#eeeeee";
    light["bg-tertiary"] = "#dddddd";
    light["text-primary"] = "#000000";
    light["text-secondary"] = "#333333";
    light["accent"] = "#0066cc";
    light["accent-dark"] = "#004488";
    light["success"] = "#228b22";
    light["error"] = "#cc0000";
    light["warning"] = "#cc8800";
    light["border"] = "#cccccc";
    ThemeManager::applyTheme(QJsonObject{{"colors", light}});

    QJsonObject seen = ThemeManager::currentColors();
    assert(seen["accent"].toString() == "#0066cc");
    assert(seen["error"].toString() == "#cc0000");

    assert(ThemeManager::styleForRole(ThemeRole::PrimaryButton).contains("#0066cc"));
    assert(ThemeManager::styleForRole(ThemeRole::DestructiveButton).contains("#cc0000"));
    assert(ThemeManager::styleForRole(ThemeRole::StatusSuccess).contains("#228b22"));
    assert(ThemeManager::styleForRole(ThemeRole::StatusWarning).contains("#cc8800"));
    assert(ThemeManager::styleForRole(ThemeRole::StatusNeutral).contains("#333333"));
    assert(ThemeManager::styleForRole(ThemeRole::BannerInfo).contains("#dddddd"));
    assert(ThemeManager::styleForRole(ThemeRole::ErrorBorder).contains("#cc0000"));

    // Prior-theme hexes must not leak into the new palette output.
    assert(!ThemeManager::styleForRole(ThemeRole::PrimaryButton).contains("#89b4fa"));
    assert(!ThemeManager::styleForRole(ThemeRole::DestructiveButton).contains("#f38ba8"));

    // Restore Catppuccin Mocha to keep later runs deterministic for manual reruns.
    ThemeManager::applyDefaultTheme();
    assert(ThemeManager::currentColors()["accent"].toString() == "#89b4fa");
    printf("  PASS: style_for_role_updates_after_apply\n");
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    printf("ThemeManager tests:\n");
    test_default_colors();
    test_palette_from_colors();
    test_different_themes_different_palettes();
    test_stylesheet_from_colors();
    test_load_from_file_valid();
    test_load_from_file_missing();
    test_empty_colors();
    test_current_colors_defaults();
    test_style_for_role_uses_default_palette();
    test_style_for_role_updates_after_apply();
    printf("All ThemeManager tests passed.\n");

    return 0;
}
