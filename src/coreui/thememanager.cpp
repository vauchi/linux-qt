// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thememanager.h"
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStyleFactory>

void ThemeManager::applyDefaultTheme() {
    applyTheme(QJsonObject{{"colors", defaultColors()}});
}

void ThemeManager::applyTheme(const QJsonObject &theme) {
    QJsonObject colors = theme["colors"].toObject();
    if (colors.isEmpty()) return;

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

    return QStringLiteral(
        "QMainWindow { background-color: %1; color: %2; }"
        "QListWidget#sidebar { background-color: %3; border-right: 1px solid %4; }"
        "QListWidget#sidebar::item:selected { background-color: %5; }"
        "QLineEdit { border: 1px solid %4; background-color: %3; color: %2; }"
        "QLabel { color: %2; }"
        "QPushButton { background-color: %5; color: %2; border: 1px solid %4; "
        "  padding: 6px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: %6; }"
    )
        .arg(bgPrimary, textPrimary, bgSecondary, border, bgTertiary, accent);
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
