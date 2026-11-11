// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QAccessible>
#include <QAction>
#include <QMenu>
#include <QPointer>
#include <QSystemTrayIcon>

using namespace display_test;

namespace {

QJsonObject statusItem(int revision = 2, bool enabled = true,
                       const QString &interaction = QStringLiteral("opaque-exchange")) {
    auto exchange = action(interaction, "Downloaded action");
    exchange.insert("accessibility_label", "Start a contact exchange");
    exchange.insert("enabled", enabled);
    return scoped("SetStatusItem", revision, "item", QJsonObject{
        {"tooltip", "Downloaded tooltip"},
        {"show_window_label", "Downloaded show"},
        {"items", QJsonArray{exchange}}});
}

QJsonObject removal(int revision = 2) {
    return scoped("SetStatusItem", revision, "item", QJsonValue::Null);
}

QSystemTrayIcon *tray() {
    const auto found = window()->findChildren<QSystemTrayIcon *>();
    assert(found.size() == 1);
    return found.first();
}

QMenu *trayMenu() {
    auto *menu = tray()->contextMenu();
    assert(menu != nullptr);
    return menu;
}

QStringList entries() {
    QStringList result;
    for (auto *item : trayMenu()->actions())
        result.append(item->isSeparator() ? QStringLiteral("|") : item->text());
    return result;
}

QAction *entry(const QString &text) {
    for (auto *item : trayMenu()->actions())
        if (item->text() == text) return item;
    assert(false);
    return nullptr;
}

QAction *nativeQuit() {
    QList<QAction *> found;
    for (auto *item : trayMenu()->actions())
        if (item->menuRole() == QAction::QuitRole) found.append(item);
    assert(found.size() == 1);
    assert(found.first()->isEnabled());
    return found.first();
}

bool shownWhenTheDesktopHasATray() {
    return tray()->isVisible() == QSystemTrayIcon::isSystemTrayAvailable();
}

QList<Step> availability() {
    return {{{}, [] {
        assert(statusItemCapabilities == (QJsonArray{event("StatusItemAvailabilityChanged",
            {{"available", QSystemTrayIcon::isSystemTrayAvailable()}})}));
        assert(!tray()->isVisible());
    }}};
}

QList<Step> prepared() {
    return {
        {{surface("first", 2, "First"), profile("first"), statusItem()}, [] {
            assert(entries() == (QStringList{"Downloaded show", "Downloaded action", "|",
                                             nativeQuit()->text()}));
            assert(tray()->toolTip() == QStringLiteral("Downloaded tooltip"));
            assert(!tray()->icon().isNull());
            assert(shownWhenTheDesktopHasATray());
            auto *item = entry("Downloaded action");
            assert(item->isEnabled());
            assert(item->menuRole() == QAction::NoRole);
            item->trigger();
        }},
        {{}, [] {
            assert(events == activation("first", "opaque-exchange"));
            events = {};
        }},
        {{removal()}, [] {
            assert(!tray()->isVisible());
            assert(events.isEmpty());
        }},
    };
}

QList<Step> accessibility() {
    return {
        {{surface("first", 2, "First"), profile("first"), statusItem()}, [] {
            auto *menu = trayMenu();
            menu->popup(QPoint(10, 10));
            menu->setActiveAction(entry("Downloaded action"));
            auto *accessible = QAccessible::queryAccessibleInterface(menu);
            assert(accessible != nullptr);
            QAccessibleInterface *item = nullptr;
            for (int index = 0; index < accessible->childCount(); ++index) {
                auto *child = accessible->child(index);
                if (child && child->object() == entry("Downloaded action")) item = child;
            }
            assert(item != nullptr);
            assert(item->role() == QAccessible::MenuItem);
            assert(item->text(QAccessible::Name) == QStringLiteral("Start a contact exchange"));
            item->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
        }},
        {{}, [] { assert(events == activation("first", "opaque-exchange")); }},
    };
}

QList<Step> showWindow() {
    return {
        {{surface("first", 2, "First"), profile("first"), statusItem()}, [] {
            auto *host = window();
            host->hide();
            entry("Downloaded show")->trigger();
            assert(host->isVisible());
            assert(events.isEmpty());
        }},
    };
}

QList<Step> revisions() {
    return {
        {{statusItem(), surface("first", 2, "First"), profile("first"), statusItem(1)}, [] {
            assert(!tray()->isVisible());
        }},
        {{statusItem()}, [] { assert(entry("Downloaded action")->isEnabled()); }},
        {{statusItem(1, false)}, [] { assert(entry("Downloaded action")->isEnabled()); }},
        {{surface("first", 3, "Next")}, [] {
            assert(entries() == (QStringList{"Downloaded show", "|", nativeQuit()->text()}));
            assert(shownWhenTheDesktopHasATray());
        }},
        {{statusItem(2)}, [] {
            assert(entries() == (QStringList{"Downloaded show", "|", nativeQuit()->text()}));
        }},
    };
}

QPointer<QAction> retired;

QList<Step> replacement() {
    return {
        {{surface("first", 2, "First"), profile("first"), statusItem()}, [] {
            retired = entry("Downloaded action");
        }},
        {{statusItem(2, false, "opaque-next")}, [] {
            auto *item = entry("Downloaded action");
            assert(!item->isEnabled());
            if (retired) retired->trigger();
            item->trigger();
        }},
        {{}, [] { assert(events.isEmpty()); }},
        {{statusItem(2, true, "opaque-next")}, [] { entry("Downloaded action")->trigger(); }},
        {{}, [] { assert(events == activation("first", "opaque-next")); }},
    };
}

QList<Step> reset() {
    return {
        {{surface("first", 2, "First"), profile("first"), statusItem()}, [] {
            retired = entry("Downloaded action");
        }},
        {{surface("first", 2, "First"), profile("first"), "ResetApplication"}, [] {
            assert(!tray()->isVisible());
            assert(!retired);
            assert(entries().size() == 3);
        }},
    };
}

QList<Step> quit() {
    return {{{surface("first", 2, "First"), profile("first"), statusItem()}, [] {
        stopAfterLastStep = false;
        nativeQuit()->trigger();
    }}};
}

} // namespace

static void scheduleVerification() { display_test::schedule(); }
Q_COREAPP_STARTUP_FUNCTION(scheduleVerification)

int main(int argc, char **argv) {
    assert(argc == 2);
    const QString test = QString::fromUtf8(argv[1]);
    if (test == "availability") steps = availability();
    else if (test == "prepared") steps = prepared();
    else if (test == "accessibility") steps = accessibility();
    else if (test == "show_window") steps = showWindow();
    else if (test == "revisions") steps = revisions();
    else if (test == "replacement") steps = replacement();
    else if (test == "reset") steps = reset();
    else if (test == "quit") steps = quit();
    else return 2;
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    return qt_display_start(argc, argv, receive, &events);
}
