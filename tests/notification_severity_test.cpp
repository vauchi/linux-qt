// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Core's `priority` field is the only input to the tray message severity;
/// the shell never branches on the notification category (ADR-066).

#include "../src/platform/notificationseverity.h"
#include <cassert>
#include <cstdio>

static void test_urgent_priority_is_critical() {
    assert(vauchi::trayIconForPriority(QStringLiteral("Urgent"))
           == QSystemTrayIcon::Critical);
    printf("  PASS: urgent_priority_is_critical\n");
}

static void test_high_priority_is_warning() {
    assert(vauchi::trayIconForPriority(QStringLiteral("High"))
           == QSystemTrayIcon::Warning);
    printf("  PASS: high_priority_is_warning\n");
}

static void test_default_priority_is_information() {
    assert(vauchi::trayIconForPriority(QStringLiteral("Default"))
           == QSystemTrayIcon::Information);
    printf("  PASS: default_priority_is_information\n");
}

// An unknown or absent hint must degrade to the quietest severity so a newer
// Core can never make an older shell shout.
static void test_unknown_priority_degrades_to_information() {
    assert(vauchi::trayIconForPriority(QStringLiteral("Loud"))
           == QSystemTrayIcon::Information);
    assert(vauchi::trayIconForPriority(QString())
           == QSystemTrayIcon::Information);
    printf("  PASS: unknown_priority_degrades_to_information\n");
}

int main() {
    printf("notification_severity_test\n");
    test_urgent_priority_is_critical();
    test_high_priority_is_warning();
    test_default_priority_is_information();
    test_unknown_priority_degrades_to_information();
    printf("ALL TESTS PASSED\n");
    return 0;
}
