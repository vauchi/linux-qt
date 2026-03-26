// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QJsonObject>
#include <QPalette>
#include <QString>

/// Maps core theme colors to QPalette for runtime-switchable theming.
///
/// Reads theme JSON (matching core's ThemeColors schema) and applies
/// colors via QPalette + QApplication::setPalette(). Bundles the same
/// Catppuccin Mocha default as vauchi-core for first-launch.
class ThemeManager {
public:
    /// Apply the default bundled theme (Catppuccin Mocha).
    static void applyDefaultTheme();

    /// Apply a theme from a JSON object (same schema as themes.json entries).
    static void applyTheme(const QJsonObject &theme);

    /// Load themes from a JSON file path and apply the first one.
    /// Returns true if a theme was successfully loaded and applied.
    static bool loadFromFile(const QString &path);

    /// Generate a QPalette from a theme colors JSON object.
    static QPalette paletteFromColors(const QJsonObject &colors);

    /// Generate a stylesheet string from theme colors for fine-grained styling.
    static QString stylesheetFromColors(const QJsonObject &colors);

    /// Returns the default theme colors as a JSON object (Catppuccin Mocha).
    static QJsonObject defaultColors();
};
