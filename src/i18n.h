// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Thin wrapper around vauchi_i18n_get_string (CABI).
///
/// Returns the translated string for the system locale, falling back
/// to @p fallback when i18n is not initialised or the key is missing.
///
/// The CABI i18n symbols are resolved at runtime via dlsym so the
/// build succeeds even when linked against an older libvauchi_cabi
/// that lacks them. Once the core i18n export merges to main the
/// dlsym indirection can be replaced with direct calls.

#pragma once

#include <QLocale>
#include <QString>
#include <dlfcn.h>
#include "vauchi.h"

// ── Runtime-resolved CABI i18n function pointers ────────────────
// Resolved once on first use via dlsym(RTLD_DEFAULT, ...).
// Returns nullptr when the loaded libvauchi_cabi lacks the symbol,
// which makes every call site fall through to its English fallback.
// ────────────────────────────────────────────────────────────────

using FnI18nGetString  = char *(*)(const char *, const char *);
using FnI18nInit       = int32_t (*)(const char *);
using FnI18nIsInit     = int32_t (*)();

struct I18nFuncs {
    FnI18nGetString get_string;
    FnI18nInit      init;
    FnI18nIsInit    is_initialized;

    static const I18nFuncs &instance() {
        static const I18nFuncs f {
            reinterpret_cast<FnI18nGetString>(
                dlsym(RTLD_DEFAULT, "vauchi_i18n_get_string")),
            reinterpret_cast<FnI18nInit>(
                dlsym(RTLD_DEFAULT, "vauchi_i18n_init")),
            reinterpret_cast<FnI18nIsInit>(
                dlsym(RTLD_DEFAULT, "vauchi_i18n_is_initialized")),
        };
        return f;
    }
};

// ── Public helpers ──────────────────────────────────────────────

inline QString systemLocaleCode() {
    // QLocale::system().name() returns e.g. "en_US"; extract "en".
    return QLocale::system().name().section('_', 0, 0);
}

inline bool vauchiI18nIsInitialized() {
    auto fn = I18nFuncs::instance().is_initialized;
    return fn ? fn() != 0 : false;
}

inline int32_t vauchiI18nInit(const char *resourceDir) {
    auto fn = I18nFuncs::instance().init;
    return fn ? fn(resourceDir) : 1;  // 1 = failure
}

inline QString tr_vauchi(const char *key, const QString &fallback) {
    auto fn = I18nFuncs::instance().get_string;
    if (!fn) return fallback;

    static const QByteArray locale = systemLocaleCode().toUtf8();
    char *result = fn(locale.constData(), key);
    if (result) {
        QString str = QString::fromUtf8(result);
        vauchi_string_free(result);
        return str;
    }
    return fallback;
}
