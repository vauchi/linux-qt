// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QKeyEvent>

using namespace display_test;

namespace {
bool verified = false;

void escape(Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Escape, modifiers);
    QApplication::sendEvent(window(), &press);
}

QJsonArray backRequested(const QString &surfaceId) {
    return {event("SurfaceActivated", {{"surface_id", surfaceId}}),
        event("BackRequested", {{"surface_id", surfaceId}})};
}

// Feature: navigation.feature — the desktop reports the active surface's gesture.
QList<Step> gesture() {
    return {{{}, [] {
        escape();
        assert(events.isEmpty());
    }}, {{surface("first", 1, "First"), surface("second", 1, "Second"),
          profile("second", true)}, [] {
        escape(Qt::AltModifier);
        assert(events.isEmpty());
        escape();
    }}, {{}, [] {
        assert(events == backRequested("second"));
        events = {};
    }}, {{profile("first", true)}, [] { escape(); }}, {{}, [] {
        assert(events == backRequested("first"));
    }}};
}

QList<Step> share() {
    const QJsonObject command{{"ShowShareSheet", QJsonObject{{"url", "vauchi://opaque-share"}}}};
    return {{{command, command}, [] {
        const auto unavailable = event("HardwareUnavailable", {{"transport", "share_sheet"}});
        assert((events == QJsonArray{unavailable, unavailable}));
        events = {};
    }}, {{QJsonObject{{"ShowShareSheet", QJsonObject{}}},
          QJsonObject{{"ShowShareSheet", QJsonObject{{"url", 9}}}}}, [] {
        assert(events.isEmpty());
    }}};
}

QList<Step> nativeBack() {
    stopAfterLastStep = false;
    return {{{"PerformNativeBack"}, [] {
        verified = true;
        assert(events.isEmpty());
    }}};
}

// @internal — assistive technology and the desktop find the app by these names.
QList<Step> identity() {
    return {{{}, [] {
        assert(QCoreApplication::applicationName() == "vauchi");
        assert(QCoreApplication::organizationName() == "vauchi");
        assert(QGuiApplication::desktopFileName() == "app.vauchi.Vauchi");
    }}};
}

// @internal — keyboard users reach commands only in an active window; a
// session without a window manager never activates it for us.
QList<Step> active() {
    return {{{}, [] { assert(QApplication::activeWindow() == window()); }}};
}

void setup() { schedule(); }
}
Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    const QString name = argc > 1 ? QString::fromUtf8(argv[1]) : "gesture";
    if (name == "gesture") steps = gesture();
    else if (name == "share") steps = share();
    else if (name == "back") steps = nativeBack();
    else if (name == "identity") steps = identity();
    else if (name == "active") steps = active();
    else return 2;
    const int result = qt_display_start(argc, argv, receive, &events);
    assert(result == 0);
    if (name == "back") assert(verified);
    return result;
}
