// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QAccessible>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>

using namespace display_test;

namespace {

QJsonObject menu(int revision = 2, bool enabled = true,
                 const QString &interaction = QStringLiteral("opaque-import")) {
    auto import = action(interaction, "Downloaded action");
    import.insert("accessibility_label", "Import a contact file");
    import.insert("enabled", enabled);
    import.insert("shortcut", "import");
    return scoped("SetApplicationMenu", revision, "menus", QJsonArray{
        QJsonObject{{"role", "file"}, {"label", "Downloaded files"},
                    {"items", QJsonArray{import}}},
        QJsonObject{{"role", "other"}, {"label", "Downloaded edits"},
                    {"items", QJsonArray{action("opaque-edit", "Downloaded edit")}}}});
}

QMenuBar *bar() {
    auto *result = window()->findChild<QMenuBar *>();
    assert(result != nullptr);
    return result;
}

QList<QMenu *> preparedMenus() {
    QList<QMenu *> result;
    for (auto *top : bar()->actions()) if (top->menu()) result.append(top->menu());
    return result;
}

QAction *nativeQuit() {
    QList<QAction *> found;
    for (auto *top : bar()->actions()) {
        if (top->menuRole() == QAction::QuitRole) found.append(top);
        if (auto *menu = top->menu()) {
            for (auto *item : menu->actions()) {
                if (item->menuRole() == QAction::QuitRole) found.append(item);
            }
        }
    }
    assert(found.size() == 1);
    assert(found.first()->isEnabled());
    assert(found.first()->shortcut() == QKeySequence(QKeySequence::Quit));
    return found.first();
}

QAction *importAction() {
    const auto menus = preparedMenus();
    assert(menus.size() == 2);
    assert(menus.first()->title() == QStringLiteral("Downloaded files"));
    return menus.first()->actions().first();
}

const QJsonArray available{
    event("ApplicationMenuAvailabilityChanged", {{"available", true}})};

QList<Step> availability() {
    return {
        {{}, [] {
            assert(capabilities == available);
            assert(preparedMenus().isEmpty());
            nativeQuit();
        }},
        {{surface("first", 2, "First"), profile("first"), menu()}, [] {
            assert(capabilities == available);
        }},
    };
}

QList<Step> prepared() {
    return {
        {{surface("first", 2, "First"), profile("first"), menu()}, [] {
            const auto menus = preparedMenus();
            assert(menus.size() == 2);
            assert(menus.last()->title() == QStringLiteral("Downloaded edits"));
            auto *item = importAction();
            assert(item->text() == QStringLiteral("Downloaded action"));
            assert(item->isEnabled());
            assert(item->menuRole() == QAction::NoRole);
            assert(item->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_I));
            auto *popup = menus.first();
            popup->popup(window()->mapToGlobal(QPoint(10, 10)));
            popup->setActiveAction(item);
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(popup, &enter);
            nativeQuit();
        }},
        {{}, [] { assert(events == activation("first", "opaque-import")); }},
    };
}

QList<Step> accessibility() {
    return {
        {{surface("first", 2, "First"), profile("first"), menu()}, [] {
            auto *popup = preparedMenus().first();
            auto *accessible = QAccessible::queryAccessibleInterface(popup);
            assert(accessible != nullptr);
            auto *item = accessible->child(0);
            assert(item && item->object() == importAction());
            assert(item->role() == QAccessible::MenuItem);
            assert(item->text(QAccessible::Name) == QStringLiteral("Import a contact file"));
            assert(!item->state().disabled);
            auto *actions = item->actionInterface();
            assert(actions != nullptr);
            actions->doAction(QAccessibleActionInterface::pressAction());
        }},
        {{}, [] { assert(events == activation("first", "opaque-import")); }},
    };
}

QList<Step> revisions() {
    return {
        {{menu(), surface("first", 2, "First"), profile("first"), menu(1)}, [] {
            assert(preparedMenus().isEmpty());
            nativeQuit();
        }},
        {{menu()}, [] { assert(importAction()->isEnabled()); }},
        {{menu(1, false)}, [] { assert(importAction()->isEnabled()); }},
        {{surface("first", 3, "Next")}, [] {
            assert(preparedMenus().isEmpty());
            nativeQuit();
        }},
        {{menu(2)}, [] { assert(preparedMenus().isEmpty()); }},
    };
}

QPointer<QAction> retired;

QList<Step> replacement() {
    return {
        {{surface("first", 2, "First"), profile("first"), menu()}, [] {
            retired = importAction();
        }},
        {{menu(2, false, "opaque-next")}, [] {
            auto *item = importAction();
            assert(!item->isEnabled());
            assert(preparedMenus().size() == 2);
            if (retired) retired->trigger();
            item->trigger();
        }},
        {{}, [] { assert(events.isEmpty()); }},
        {{menu(2, true, "opaque-next")}, [] { importAction()->trigger(); }},
        {{}, [] {
            assert(events == activation("first", "opaque-next"));
            events = {};
        }},
        {{scoped("SetApplicationMenu", 2, "menus", QJsonArray{})}, [] {
            assert(preparedMenus().isEmpty());
            nativeQuit();
            assert(events.isEmpty());
        }},
    };
}

QList<Step> quit() {
    return {{{surface("first", 2, "First"), profile("first"), menu(),
              scoped("SetApplicationMenu", 2, "menus", QJsonArray{})}, [] {
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
    else if (test == "revisions") steps = revisions();
    else if (test == "replacement") steps = replacement();
    else if (test == "quit") steps = quit();
    else return 2;
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    return qt_display_start(argc, argv, receive, &events);
}
