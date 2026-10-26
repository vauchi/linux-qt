// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../src/platform/menubar.h"
#include <QAction>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenu>
#include <cassert>
#include <cstdio>

static QList<QAction *> shortcuts(VauchiMenuBar &bar, const QKeySequence &key) {
    QList<QAction *> result;
    for (QAction *top : bar.actions()) {
        if (auto *menu = top->menu()) {
            for (QAction *item : menu->actions()) {
                if (item->shortcut() == key) result.append(item);
            }
        }
    }
    return result;
}

static QJsonObject preparedMenu(bool enabled, const QString &id = "opaque-27") {
    return {
        {"surface_id", "opaque-surface"}, {"revision", 3},
        {"menus", QJsonArray{QJsonObject{
            {"role", "file"}, {"label", "Downloaded files"},
            {"items", QJsonArray{QJsonObject{
                {"interaction_id", id}, {"label", "Downloaded action"},
                {"accessibility_label", "Downloaded accessible action"},
                {"enabled", enabled}, {"shortcut", "import"},
                {"icon_token", QJsonValue::Null}, {"tone", "standard"}
            }}}
        }}}
    };
}

static QAction *nativeQuit(VauchiMenuBar &bar) {
    QList<QAction *> result;
    for (QAction *top : bar.actions()) {
        if (auto *menu = top->menu()) {
            for (QAction *item : menu->actions()) {
                if (item->menuRole() == QAction::QuitRole) result.append(item);
            }
        }
    }
    assert(result.size() == 1);
    return result[0];
}

static void prepared_copy_and_opaque_activation() {
    VauchiMenuBar bar;
    assert(shortcuts(bar, QKeySequence(Qt::CTRL | Qt::Key_I)).isEmpty());
    bar.applyApplicationMenu(preparedMenu(true));
    const auto items = shortcuts(bar, QKeySequence(Qt::CTRL | Qt::Key_I));
    assert(items.size() == 1);
    assert(items[0]->text() == "Downloaded action");
    assert(items[0]->isEnabled());
    assert(bar.actions()[0]->menu()->title() == "Downloaded files");
    QString observedSurface, observedAction;
    QObject::connect(&bar, &VauchiMenuBar::actionActivated,
                     [&observedSurface, &observedAction](const QString &surface,
                                                        const QString &action) {
        observedSurface = surface;
        observedAction = action;
    });
    items[0]->trigger();
    assert(observedSurface == "opaque-surface");
    assert(observedAction == "opaque-27");
}

static void replacement_disabled_state_and_native_quit() {
    VauchiMenuBar bar;
    int activations = 0;
    QObject::connect(&bar, &VauchiMenuBar::actionActivated,
                     [&activations](const QString &, const QString &) { ++activations; });
    bar.applyApplicationMenu(preparedMenu(true));
    bar.applyApplicationMenu(preparedMenu(false, "opaque-next"));
    const auto items = shortcuts(bar, QKeySequence(Qt::CTRL | Qt::Key_I));
    assert(items.size() == 1);
    assert(!items[0]->isEnabled());
    items[0]->trigger();
    assert(activations == 0);
    assert(nativeQuit(bar)->isEnabled());
    bar.applyApplicationMenu(QJsonObject{{"menus", QJsonArray{}}});
    assert(shortcuts(bar, QKeySequence(Qt::CTRL | Qt::Key_I)).isEmpty());
    QAction *quit = nativeQuit(bar);
    int quits = 0;
    QObject::connect(&bar, &VauchiMenuBar::quitRequested, [&quits]() { ++quits; });
    quit->trigger();
    assert(quits == 1);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    prepared_copy_and_opaque_activation();
    replacement_disabled_state_and_native_quit();
    printf("ALL TESTS PASSED\n");
    return 0;
}
