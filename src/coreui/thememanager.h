// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QJsonObject>
#include <QPalette>
#include <QString>

/// Semantic style role. Components request CSS by role; ThemeManager
/// returns a snippet derived from the currently applied theme palette so
/// no component embeds hardcoded hex colors.
enum class ThemeRole {
    PrimaryButton,      ///< Affirmative action button (save, confirm)
    DestructiveButton,  ///< Irrevocable destructive action button
    StatusSuccess,      ///< Status indicator dot — success
    StatusError,        ///< Status indicator dot — failure
    StatusWarning,      ///< Status indicator dot — warning
    StatusInProgress,   ///< Status indicator dot — in progress
    StatusNeutral,      ///< Status indicator dot — pending/unknown
    DestructiveText,    ///< Red text (warning copy, destructive link)
    SecondaryText,      ///< Muted/caption/subtitle text
    BannerInfo,         ///< Informational banner background container
    ErrorBorder,        ///< Validation-error border overlay (appended to existing style)
};

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

    /// Returns the colors from the most-recently applied theme, falling back
    /// to defaultColors() until applyTheme()/applyDefaultTheme() is called.
    static QJsonObject currentColors();

    /// Returns a Qt stylesheet snippet for the given semantic role, with
    /// colors resolved from currentColors(). Components use this instead of
    /// embedding hex literals so theme switches propagate everywhere.
    static QString styleForRole(ThemeRole role);
};
