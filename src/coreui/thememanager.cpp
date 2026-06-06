// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thememanager.h"
#include "Tokens.h"
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStyleFactory>

namespace {
// Cached copy of the most-recently applied palette so styleForRole() can
// resolve colors without reloading themes.json. Defaults to Catppuccin Mocha
// until applyTheme() runs.
QJsonObject &mutableCurrentColors() {
    static QJsonObject colors = ThemeManager::defaultColors();
    return colors;
}
}

QFont ThemeManager::uiFont() {
    // "DejaVu Sans" is a hard transitive dependency of every Linux
    // desktop and the CI snapshot container, so pinning it by name needs
    // no bundled asset. The SansSerif style hint steers Qt's substitution
    // toward a proportional family if the host somehow lacks it — never
    // back to the monospace default that caused the font-drift.
    QFont font(QStringLiteral("DejaVu Sans"));
    font.setStyleHint(QFont::SansSerif);
    return font;
}

QString ThemeManager::fontFamilyCss() {
    return QStringLiteral("font-family: \"%1\"; ").arg(uiFont().family());
}

void ThemeManager::applyDefaultTheme() {
    applyTheme(QJsonObject{{"colors", defaultColors()}});
}

void ThemeManager::applyDefaultLightTheme() {
    applyTheme(QJsonObject{{"colors", defaultLightColors()}});
}

void ThemeManager::applyTheme(const QJsonObject &theme) {
    QJsonObject colors = theme["colors"].toObject();
    if (colors.isEmpty()) return;

    mutableCurrentColors() = colors;

    QPalette palette = paletteFromColors(colors);
    QApplication::setPalette(palette);
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QString stylesheet = stylesheetFromColors(colors);
    if (auto *app = qApp) {
        app->setStyleSheet(stylesheet);
    }
}

bool ThemeManager::loadFromFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) return false;

    QJsonArray themes = doc.array();
    if (themes.isEmpty()) return false;

    applyTheme(themes.first().toObject());
    return true;
}

QPalette ThemeManager::paletteFromColors(const QJsonObject &colors) {
    QPalette palette;

    QColor bgPrimary(colors["bg-primary"].toString());
    QColor bgSecondary(colors["bg-secondary"].toString());
    QColor bgTertiary(colors["bg-tertiary"].toString());
    QColor textPrimary(colors["text-primary"].toString());
    QColor textSecondary(colors["text-secondary"].toString());
    QColor accent(colors["accent"].toString());
    QColor errorColor(colors["error"].toString());
    QColor border(colors["border"].toString());

    // Window / base colors
    palette.setColor(QPalette::Window, bgPrimary);
    palette.setColor(QPalette::WindowText, textPrimary);
    palette.setColor(QPalette::Base, bgSecondary);
    palette.setColor(QPalette::AlternateBase, bgTertiary);
    palette.setColor(QPalette::Text, textPrimary);
    palette.setColor(QPalette::BrightText, textPrimary);

    // Buttons
    palette.setColor(QPalette::Button, bgTertiary);
    palette.setColor(QPalette::ButtonText, textPrimary);

    // Selection / highlight
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, bgPrimary);

    // Links
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accent.darker(120));

    // Placeholder text
    palette.setColor(QPalette::PlaceholderText, textSecondary);

    // Tooltips
    palette.setColor(QPalette::ToolTipBase, bgTertiary);
    palette.setColor(QPalette::ToolTipText, textPrimary);

    // Mid / shadow / dark for frames and borders
    palette.setColor(QPalette::Mid, border);
    palette.setColor(QPalette::Dark, bgSecondary);
    palette.setColor(QPalette::Shadow, bgSecondary.darker(150));
    palette.setColor(QPalette::Light, bgTertiary.lighter(120));
    palette.setColor(QPalette::Midlight, bgTertiary);

    return palette;
}

QString ThemeManager::stylesheetFromColors(const QJsonObject &colors) {
    QString bgPrimary = colors["bg-primary"].toString();
    QString bgSecondary = colors["bg-secondary"].toString();
    QString bgTertiary = colors["bg-tertiary"].toString();
    QString textPrimary = colors["text-primary"].toString();
    QString textSecondary = colors["text-secondary"].toString();
    QString accent = colors["accent"].toString();
    QString errorColor = colors["error"].toString();
    QString border = colors["border"].toString();

    // A QWidget base rule pins the font-family globally. The per-component
    // `setStyleSheet("font-size: …")` calls would otherwise reset the
    // family to Qt's style default (which QApplication::setFont cannot
    // override once a widget stylesheet sets any font property) — the
    // root cause of the snapshot font-drift
    // (2026-06-05-linux-qt-snapshot-find-app-collision).
    return QStringLiteral(
        "QWidget { font-family: \"%7\"; }"
        "QMainWindow { background-color: %1; color: %2; }"
        "QListWidget#sidebar { background-color: %3; border-right: 1px solid %4; }"
        "QListWidget#sidebar::item:selected { background-color: %5; }"
        "QLineEdit { border: 1px solid %4; background-color: %3; color: %2; }"
        "QLabel { color: %2; }"
        "QPushButton { background-color: %5; color: %2; border: 1px solid %4; "
        "  padding: 6px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: %6; }"
    )
        .arg(bgPrimary, textPrimary, bgSecondary, border, bgTertiary, accent,
             uiFont().family());
}

QJsonObject ThemeManager::currentColors() {
    return mutableCurrentColors();
}

QString ThemeManager::styleForRole(ThemeRole role) {
    const QJsonObject colors = currentColors();
    const QString accent      = colors["accent"].toString();
    const QString error       = colors["error"].toString();
    const QString success     = colors["success"].toString();
    const QString warning     = colors["warning"].toString();
    const QString textPrimary = colors["text-primary"].toString();
    const QString textSecondary = colors["text-secondary"].toString();
    const QString bgTertiary  = colors["bg-tertiary"].toString();

    // Spacing, radius, and type sizes flow from design tokens (Tokens.h)
    // so a single source of truth governs both the Rust core and this frontend.
    const int radiusSm   = Tokens::BorderRadius::SM;    // buttons, banner
    const int padV       = Tokens::Spacing::SM;         // 8px — vertical button padding
    const int padH       = Tokens::Spacing::MD;         // 16px — horizontal button padding
    const int bannerPad  = Tokens::Spacing::SM;         // banner content padding
    const int statusSize = Tokens::Typography::CAPTION_SIZE; // status dot glyph size

    switch (role) {
    case ThemeRole::PrimaryButton:
        return QStringLiteral("color: %1; background-color: %2; "
                              "border-radius: %3px; padding: %4px %5px;")
            .arg(textPrimary, accent)
            .arg(radiusSm).arg(padV).arg(padH);
    case ThemeRole::DestructiveButton:
        return QStringLiteral("color: %1; background-color: %2; "
                              "border-radius: %3px; padding: %4px %5px;")
            .arg(textPrimary, error)
            .arg(radiusSm).arg(padV).arg(padH);
    case ThemeRole::StatusSuccess:
        return QStringLiteral("color: %1; font-size: %2px;").arg(success).arg(statusSize);
    case ThemeRole::StatusError:
        return QStringLiteral("color: %1; font-size: %2px;").arg(error).arg(statusSize);
    case ThemeRole::StatusWarning:
        return QStringLiteral("color: %1; font-size: %2px;").arg(warning).arg(statusSize);
    case ThemeRole::StatusInProgress:
        return QStringLiteral("color: %1; font-size: %2px;").arg(accent).arg(statusSize);
    case ThemeRole::StatusNeutral:
        return QStringLiteral("color: %1; font-size: %2px;").arg(textSecondary).arg(statusSize);
    case ThemeRole::DestructiveText:
        return QStringLiteral("color: %1;").arg(error);
    case ThemeRole::SecondaryText:
        return QStringLiteral("color: %1;").arg(textSecondary);
    case ThemeRole::BannerInfo:
        return QStringLiteral("background-color: %1; border-radius: %2px; padding: %3px;")
            .arg(bgTertiary).arg(radiusSm).arg(bannerPad);
    case ThemeRole::ErrorBorder:
        return QStringLiteral("border: 2px solid %1;").arg(error);
    }
    return QString();
}

QJsonObject ThemeManager::defaultColors() {
    // Matches vauchi-core's default_theme() — Catppuccin Mocha
    QJsonObject colors;
    colors["bg-primary"]     = QStringLiteral("#1e1e2e");
    colors["bg-secondary"]   = QStringLiteral("#181825");
    colors["bg-tertiary"]    = QStringLiteral("#313244");
    colors["text-primary"]   = QStringLiteral("#cdd6f4");
    colors["text-secondary"] = QStringLiteral("#a6adc8");
    colors["accent"]         = QStringLiteral("#89b4fa");
    colors["accent-dark"]    = QStringLiteral("#74c7ec");
    colors["success"]        = QStringLiteral("#a6e3a1");
    colors["error"]          = QStringLiteral("#f38ba8");
    colors["warning"]        = QStringLiteral("#fab387");
    colors["border"]         = QStringLiteral("#45475a");
    return colors;
}

QJsonObject ThemeManager::defaultLightColors() {
    // Catppuccin Latte — the light counterpart to defaultColors()'s
    // Catppuccin Mocha. Values resolved from themes/themes.json
    // `catppuccin-latte` (semantic → primitive map) at the time of
    // writing; mirror updates here when themes.json drifts.
    QJsonObject colors;
    colors["bg-primary"]     = QStringLiteral("#eff1f5");
    colors["bg-secondary"]   = QStringLiteral("#e6e9ef");
    colors["bg-tertiary"]    = QStringLiteral("#ccd0da");
    colors["text-primary"]   = QStringLiteral("#4c4f69");
    colors["text-secondary"] = QStringLiteral("#6a6d82");
    colors["accent"]         = QStringLiteral("#1e66f5");
    colors["accent-dark"]    = QStringLiteral("#209fb5");
    colors["success"]        = QStringLiteral("#40a02b");
    colors["error"]          = QStringLiteral("#d20f39");
    colors["warning"]        = QStringLiteral("#fe640b");
    colors["border"]         = QStringLiteral("#9ca0b0");
    return colors;
}
