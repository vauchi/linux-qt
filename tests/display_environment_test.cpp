// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <cstdio>

using namespace display_test;

namespace {
QJsonObject environment;
QList<QJsonObject> reports;

void receiveEnvironment(const char *json, void *) {
    const auto event = QJsonDocument::fromJson(json).object();
    if (event.contains("PresentationEnvironmentChanged")) {
        environment = event.value("PresentationEnvironmentChanged").toObject();
        reports.append(environment);
    }
}

void verifyStartup(void *) {
    assert(!environment.isEmpty());
    assert(environment.value("available_width").toInt() == window()->width());
    assert(environment.value("available_height").toInt() == window()->height());
    assert(environment.value("input_modes").toArray()
           == QJsonArray({"pointer", "keyboard"}));
    assert(environment.value("motion").toString()
           == (qEnvironmentVariableIsSet("QT_REDUCE_MOTION") ? "reduced" : "full"));
    window()->resize(640, 480);
    QMetaObject::invokeMethod(qApp, [] {
        assert(environment.value("available_width").toInt() == 640);
        assert(environment.value("available_height").toInt() == 480);
        assert(reports.size() >= 2);
        qt_display_stop();
    }, Qt::QueuedConnection);
}
}

// @scenario: generic_presentation_protocol.feature :: Available window drives structural composition
int main(int argc, char **argv) {
    return qt_display_start_with_poll(argc, argv, receiveEnvironment, verifyStartup, nullptr);
}
