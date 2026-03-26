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
    printf("All ThemeManager tests passed.\n");

    return 0;
}
