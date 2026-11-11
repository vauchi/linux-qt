// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// A vauchi:// link reaches Core as DeepLinkOpened, whether the desktop
// entry passed it to this launch or to a later one that handed it over
// (vauchi/private#650, #1029). The legacy qvauchi did this in main.cpp;
// the display library now does it for the app that assembles it.

#include "qt_display.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

namespace {
const char *const kLaunchLink = "vauchi://exchange?pk=AAAA&n=BBBB";
const char *const kRelayedLink = "vauchi://exchange?pk=CCCC&n=DDDD";

struct State {
    std::vector<std::string> links;
    int polls = 0;
    QProcess *second = nullptr;
    int secondExit = -1;
};

void event(const char *json, void *context) {
    auto &state = *static_cast<State *>(context);
    const QJsonObject event = QJsonDocument::fromJson(json).object();
    if (event.contains("DeepLinkOpened")) {
        state.links.push_back(
            event.value("DeepLinkOpened").toObject().value("uri").toString().toStdString());
    }
}

void launchPoll(void *context) {
    auto &state = *static_cast<State *>(context);
    assert(state.links == std::vector<std::string>{kLaunchLink});
    qt_display_stop();
}

void relayPoll(void *context) {
    auto &state = *static_cast<State *>(context);
    if (++state.polls == 1) {
        assert(state.links.empty());
        state.second = new QProcess(qApp);
        QObject::connect(state.second, &QProcess::finished, qApp,
                         [&state](int code) { state.secondExit = code; });
        state.second->start(QCoreApplication::applicationFilePath(),
                            {QStringLiteral("hand-off"), QString::fromUtf8(kRelayedLink)});
        return;
    }
    if (state.secondExit == -1 || state.links.empty()) return;
    assert(state.secondExit == 0);
    assert(state.links == std::vector<std::string>{kRelayedLink});
    qt_display_stop();
}

void watchdog() {
    if (QCoreApplication::arguments().value(1) != QStringLiteral("hand-off"))
        QTimer::singleShot(10000, [] { qApp->exit(42); });
}
} // namespace
Q_COREAPP_STARTUP_FUNCTION(watchdog)

// @internal — cases: launch, relay, alone; hand-off is the relay's second launch.
int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "hand-off") return qt_display_hand_off_links(argc, argv) == 1 ? 0 : 3;
    if (mode == "alone") {
        char *args[] = {argv[0], const_cast<char *>(kLaunchLink)};
        assert(qt_display_hand_off_links(2, args) == 0);
        char *noLink[] = {argv[0], const_cast<char *>("--flag")};
        assert(qt_display_hand_off_links(2, noLink) == 0);
        return 0;
    }
    State state;
    if (mode == "launch") {
        char *args[] = {argv[0], argv[1], const_cast<char *>("--flag"),
                        const_cast<char *>(kLaunchLink),
                        const_cast<char *>("https://example.org/not-ours")};
        assert(qt_display_start_with_poll(5, args, event, launchPoll, &state) == 0);
        return 0;
    }
    assert(mode == "relay");
    assert(qt_display_start_with_poll(argc, argv, event, relayPoll, &state) == 0);
    assert(state.links == std::vector<std::string>{kRelayedLink});
    return 0;
}
