// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// The shell reports what the network costs and Core decides whether a
/// "Wi-Fi only" content update may run (vauchi/private#733 D6). What the
/// shell cannot tell is unknown, which Core counts as metered.

#include "../src/platform/networkcost.h"
#include <QJsonDocument>
#include <cassert>
#include <cstdio>

static void test_a_metered_connection_is_metered() {
    assert(vauchi::networkCostName(true, true, true) == QStringLiteral("metered"));
    printf("  PASS: a_metered_connection_is_metered\n");
}

static void test_an_unmetered_connection_is_unmetered() {
    assert(vauchi::networkCostName(true, true, false) == QStringLiteral("unmetered"));
    printf("  PASS: an_unmetered_connection_is_unmetered\n");
}

static void test_no_connection_is_unknown() {
    assert(vauchi::networkCostName(true, false, false) == QStringLiteral("unknown"));
    printf("  PASS: no_connection_is_unknown\n");
}

// Without NetworkManager, Qt's backend cannot tell metered from not.
static void test_a_backend_without_metering_is_unknown() {
    assert(vauchi::networkCostName(false, true, false) == QStringLiteral("unknown"));
    printf("  PASS: a_backend_without_metering_is_unknown\n");
}

static void test_the_event_is_the_one_core_decodes() {
    const QByteArray json =
        QJsonDocument(vauchi::networkCostEvent(QStringLiteral("metered")))
            .toJson(QJsonDocument::Compact);
    assert(json == R"({"NetworkCostChanged":{"cost":"metered"}})");
    printf("  PASS: the_event_is_the_one_core_decodes\n");
}

int main() {
    printf("network_cost_test\n");
    test_a_metered_connection_is_metered();
    test_an_unmetered_connection_is_unmetered();
    test_no_connection_is_unknown();
    test_a_backend_without_metering_is_unknown();
    test_the_event_is_the_one_core_decodes();
    printf("ALL TESTS PASSED\n");
    return 0;
}
