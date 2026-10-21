// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// The OS language reaches Core unparsed so "Follow system" renders in it;
/// Core alone maps the tag to a language (vauchi/private#721, ADR-066).

#include "../src/platform/systemlocale.h"
#include <cassert>
#include <cstdio>

static void test_system_language_is_reported_as_its_tag() {
    assert(vauchi::systemLocaleContextJson(QLocale(QLocale::French, QLocale::Switzerland))
           == QByteArray(R"({"system_locale":"fr-CH"})"));
    printf("  PASS: system_language_is_reported_as_its_tag\n");
}

// The C locale names no language, so Core falls back on its own.
static void test_c_locale_reports_no_system_language() {
    assert(vauchi::systemLocaleContextJson(QLocale::c()) == QByteArray("{}"));
    printf("  PASS: c_locale_reports_no_system_language\n");
}

int main() {
    printf("system_locale_test\n");
    test_system_language_is_reported_as_its_tag();
    test_c_locale_reports_no_system_language();
    return 0;
}
