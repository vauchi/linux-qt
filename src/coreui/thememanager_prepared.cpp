// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thememanager.h"
#include <QApplication>
#include <QMap>
#include <QRegularExpression>

bool ThemeManager::applyPreparedTokens(const QJsonObject &tokens) {
    const auto supplied = tokens.value("colors").toObject();
    static const QRegularExpression hex(QStringLiteral("\\A#[0-9a-fA-F]{6}\\z"));
    const auto valid = [](const QJsonValue &value) {
        return value.isString() && hex.match(value.toString()).hasMatch();
    };
    const QMap<QString, QString> roles{{"background", "bg-primary"}, {"surface", "bg-secondary"},
        {"text", "text-primary"}, {"text_muted", "text-secondary"}, {"accent", "accent"},
        {"border", "border"}, {"success", "success"}, {"warning", "warning"}, {"error", "error"}};
    QJsonObject colors;
    for (auto role = roles.begin(); role != roles.end(); ++role) {
        if (!valid(supplied.value(role.key()))) return false;
        colors.insert(role.value(), supplied.value(role.key()).toString().toLower());
    }
    const auto optional = [&](const QString &source, const QString &target, const QString &fallback) {
        const auto value = supplied.value(source);
        if (value.isNull() || value.isUndefined()) colors.insert(target, fallback);
        else if (valid(value)) colors.insert(target, value.toString().toLower());
        else return false;
        return true;
    };
    const QString surface = colors.value("bg-secondary").toString();
    if (!optional("surface_input", "input", surface)
        || !optional("surface_muted", "surface-muted", surface)
        || !optional("surface_elevated", "surface-elevated", surface)
        || !optional("text_on_accent", "text-on-accent", colors.value("bg-primary").toString())
        || !optional("focus_ring", "focus-ring", colors.value("accent").toString())) return false;
    colors.insert("bg-tertiary", colors.value("input"));
    colors.insert("accent-dark", colors.value("accent"));
    const auto fontValue = tokens.value("fonts");
    if (!fontValue.isNull() && !fontValue.isUndefined() && !fontValue.isObject()) return false;
    const auto fonts = fontValue.toObject();
    if (fontValue.isObject())
        for (const char *role : {"display", "body", "mono"})
            if (!fonts.value(role).isString()) return false;
    const QString sheet = stylesheetFromColors(colors, fonts);
    if (currentColors() == colors && qApp->styleSheet() == sheet
        && QApplication::palette() == paletteFromColors(colors)) return true;
    applyTheme(QJsonObject{{"colors", colors}, {"fonts", fonts}}, true);
    return true;
}
