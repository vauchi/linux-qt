// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_display.h"
#include <QApplication>
#include <QJsonDocument>
#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <cassert>
#include <cstring>

namespace {
struct State {
    QThread *thread = QThread::currentThread();
    int polls = 0;
    bool eventReceived = false;
};
void event(const char *json, void *context) {
    auto &state = *static_cast<State *>(context);
    assert(QThread::currentThread() == state.thread);
    assert(QJsonDocument::fromJson(json).isObject());
    state.eventReceived = true;
}
void poll(void *context) {
    auto &state = *static_cast<State *>(context);
    assert(QThread::currentThread() == state.thread);
    assert(state.eventReceived);
    ++state.polls;
    if (state.polls == 1) {
        const char *commands = R"([{"SetPresentationProfile":{"profile":{"window_title":"Tick update","window_class":"compact","pane_layout":"single","primary_surface":"tick","active_surface":"tick","detail_surface":null}}}])";
        assert(qt_display_render(commands, std::strlen(commands)) == 0);
    } else {
        assert(state.polls == 2);
        QList<QMainWindow *> windows;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (auto *window = qobject_cast<QMainWindow *>(widget); window && window->isVisible())
                windows.append(window);
        }
        assert(windows.size() == 1);
        assert(windows.first()->windowTitle() == QStringLiteral("Tick update"));
        qt_display_stop();
    }
}
void watchdog() {
    QTimer::singleShot(5000, [] { qApp->exit(42); });
}
}
Q_COREAPP_STARTUP_FUNCTION(watchdog)

// @internal — the app must make progress on Qt's thread without user input.
int main(int argc, char **argv) {
    State state;
    assert(qt_display_render("[]", 2) == -1);
    assert(qt_display_start_with_poll(argc, argv, event, poll, &state) == 0);
    assert(state.polls == 2);
    assert(qt_display_render("[]", 2) == -1);
    qt_display_stop();
}
