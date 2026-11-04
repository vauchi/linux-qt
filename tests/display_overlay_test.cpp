// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QAccessible>
#include <QDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>

using namespace display_test;

namespace {

QJsonObject overlay(const QString &kind, int revision = 2) {
    return scoped("PresentOverlay", revision, "overlay", QJsonObject{
        {"kind", kind}, {"title", "Prepared help"}, {"body", "Prepared explanation"},
        {"items", QJsonArray{action("choose", "Choose")}}});
}

QList<QDialog *> dialogs() {
    QList<QDialog *> shown;
    for (auto *dialog : window()->findChildren<QDialog *>()) {
        if (dialog->isVisible()) shown.append(dialog);
    }
    return shown;
}

QList<Step> information() {
    return {
        {{surface("first", 2, "First"), profile("first"), overlay("information", 1)}, [] {
            assert(dialogs().isEmpty());
            assert(events.isEmpty());
        }},
        {{overlay("information")}, [] {
            const auto shown = dialogs();
            assert(shown.size() == 1);
            auto *dialog = shown.first();
            assert(dialog->windowTitle() == QStringLiteral("Prepared help"));
            const auto labels = dialog->findChildren<QLabel *>();
            assert(labels.size() == 1);
            assert(labels.first()->text() == QStringLiteral("Prepared explanation"));
            const auto buttons = dialog->findChildren<QPushButton *>();
            assert(buttons.size() == 1);
            assert(buttons.first()->text().isEmpty());
            assert(!buttons.first()->icon().isNull());
            auto *accessible = QAccessible::queryAccessibleInterface(buttons.first());
            assert(accessible && accessible->text(QAccessible::Name) == QStringLiteral("Prepared help"));
            buttons.first()->click();
        }},
        {{}, [] {
            assert(dialogs().isEmpty());
            assert((events == QJsonArray{event("OverlayDismissed",
                {{"surface_id", "first"}, {"kind", "information"}})}));
        }},
    };
}

QList<Step> navigation() {
    return {
        {{surface("first", 2, "First"), profile("first"), overlay("navigation")}, [] {
            const auto shown = dialogs();
            assert(shown.size() == 1);
            const auto buttons = shown.first()->findChildren<QPushButton *>();
            assert(buttons.size() == 1);
            assert(buttons.first()->text() == QStringLiteral("Choose"));
            buttons.first()->click();
        }},
        {{}, [] {
            assert(dialogs().isEmpty());
            assert(events == activation("first", "choose"));
        }},
    };
}

QList<Step> menu() {
    return {
        {{surface("first", 2, "First"), profile("first"), overlay("action_menu")}, [] {
            auto *menu = window()->findChild<QMenu *>();
            assert(menu && menu->isVisible());
            const auto actions = menu->actions();
            assert(actions.size() == 1);
            assert(actions.first()->text() == QStringLiteral("Choose"));
            actions.first()->trigger();
            menu->close();
        }},
        {{}, [] { assert(events == activation("first", "choose")); }},
    };
}

QList<Step> dismissal() {
    return {
        {{surface("first", 2, "First"), profile("first"), overlay("information")}, [] {
            assert(dialogs().size() == 1);
        }},
        {{scoped("DismissOverlay", 1, "kind", "information"),
          scoped("DismissOverlay", 2, "kind", "navigation"),
          scoped("DismissOverlay", 2, "kind", "information", "second")}, [] {
            assert(dialogs().size() == 1);
            assert(events.isEmpty());
        }},
        {{scoped("DismissOverlay", 2, "kind", "information")}, [] {
            assert(dialogs().isEmpty());
            assert(events.isEmpty());
        }},
        {{scoped("DismissOverlay", 2, "kind", "information")}, [] {
            assert(dialogs().isEmpty());
            assert(events.isEmpty());
        }},
        {{overlay("information"), surface("first", 3, "Next")}, [] {
            assert(dialogs().isEmpty());
            assert(events.isEmpty());
        }},
    };
}

} // namespace

static void scheduleVerification() { display_test::schedule(); }
Q_COREAPP_STARTUP_FUNCTION(scheduleVerification)

int main(int argc, char **argv) {
    assert(argc == 2);
    const QString test = QString::fromUtf8(argv[1]);
    if (test == "information") steps = information();
    else if (test == "navigation") steps = navigation();
    else if (test == "menu") steps = menu();
    else if (test == "dismissal") steps = dismissal();
    else return 2;
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    return qt_display_start(argc, argv, receive, &events);
}
