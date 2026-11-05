// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"

using namespace display_test;

// Feature: contact_exchange.feature — desktop screen capability observations.
void setup() {
    const QList<QPair<QString, QJsonObject>> requests{
        {"SetScreenBrightness", {{"level", 0.65}}},
        {"SetScreenBrightness", {{"level", QJsonValue::Null}}},
        {"SetIdleTimerDisabled", {{"disabled", true}}},
        {"SetIdleTimerDisabled", {{"disabled", false}}},
        {"SetOrientationLock", {{"orientation", "portrait"}}},
        {"SetOrientationLock", {{"orientation", QJsonValue::Null}}},
    };
    for (const auto &request : requests) {
        const QString transport = request.first == "SetScreenBrightness" ? "screen_brightness"
            : request.first == "SetIdleTimerDisabled" ? "idle_timer" : "orientation_lock";
        steps.append({QJsonArray{QJsonObject{{request.first, request.second}}}, [transport] {
            assert(events == QJsonArray{event("HardwareUnavailable", {{"transport", transport}})});
            events = {};
        }});
    }
    steps.append({QJsonArray{QJsonObject{{"UnknownCommand", QJsonObject{}}}}, [] {
        assert(events == QJsonArray{});
    }});
    schedule();
}

Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    const int result = qt_display_start(argc, argv, receive, &events);
    assert(result == 0);
    return result;
}
