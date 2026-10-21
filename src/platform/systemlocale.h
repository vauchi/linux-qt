// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Reports the OS language tag to Core, unparsed, so "Follow system"
/// renders in it; Core alone maps the tag to a language
/// (vauchi/private#721, ADR-066).

#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <cstdint>
#include <dlfcn.h>

namespace vauchi {

inline QByteArray systemLocaleContextJson(const QLocale &locale) {
    QJsonObject context;
    if (locale.language() != QLocale::C) {
        context[QStringLiteral("system_locale")] = locale.bcp47Name();
    }
    return QJsonDocument(context).toJson(QJsonDocument::Compact);
}

/// Resolved at runtime, like the i18n calls in i18n.h, so a Core release
/// without the call starts as before.
inline void reportSystemLocale(void *app, const QLocale &locale = QLocale::system()) {
    using SetRenderContextJson = int32_t (*)(void *, const char *);
    static const auto setRenderContextJson = reinterpret_cast<SetRenderContextJson>(
        dlsym(RTLD_DEFAULT, "vauchi_app_set_render_context_json"));
    if (app && setRenderContextJson) {
        setRenderContextJson(app, systemLocaleContextJson(locale).constData());
    }
}

} // namespace vauchi
