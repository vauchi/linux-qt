// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>

using namespace display_test;

namespace {

bool hasTitle(const QString &title) {
    for (auto *label : window()->findChildren<QLabel *>()) {
        if (label->text() == title && label->isVisible()) return true;
    }
    return false;
}

QJsonArray input(const QString &value) {
    return {QJsonObject{{"Input", QJsonObject{
        {"binding_id", "name"}, {"label", "Name"}, {"value", value},
        {"input_kind", "text"}, {"enabled", true},
        {"accessibility", QJsonObject{{"label", "Name"}}}}}}};
}

QLineEdit *field() {
    auto *line = window()->findChild<QLineEdit *>(QStringLiteral("name"));
    assert(line != nullptr);
    return line;
}

QPushButton *button(const QString &text) {
    for (auto *candidate : window()->findChildren<QPushButton *>()) {
        if (candidate->text() == text) return candidate;
    }
    return nullptr;
}

QJsonObject context(int revision, const QString &label) {
    return scoped("SetContextBar", revision, "bar",
                  QJsonObject{{"primary", action("save", label)}});
}

QList<Step> composition() {
    return {
        {{surface("first", 1, "First"), surface("second", 1, "Second"),
          profile("first", true)}, [] {
            assert(hasTitle("First"));
            assert(hasTitle("Second"));
        }},
        {{profile("first")}, [] {
            assert(hasTitle("First"));
            assert(!hasTitle("Second"));
        }},
        {{profile("second")}, [] {
            assert(!hasTitle("First"));
            assert(hasTitle("Second"));
        }},
    };
}

QList<Step> contextBar() {
    return {
        {{surface("first", 2, "Current"), profile("first"), context(2, "Save")}, [] {
            auto *save = button("Save");
            assert(save != nullptr && save->isVisible());
            save->click();
        }},
        {{}, [] { assert(events == activation("first", "save")); }},
        {{context(1, "Stale")}, [] {
            assert(button("Stale") == nullptr);
            assert(button("Save") != nullptr);
        }},
        {{context(2, "Updated")}, [] {
            assert(button("Save") == nullptr);
            assert(button("Updated") != nullptr);
        }},
    };
}

QList<Step> navigation() {
    const auto nav = scoped("SetNavigation", 1, "navigation",
        QJsonObject{{"items", QJsonArray{QJsonObject{
            {"interaction_id", "contacts"}, {"label", "Contacts"},
            {"accessibility_label", "Open contacts"}, {"selected", true}}}}});
    return {
        {{surface("first", 1, "First"), profile("first"), nav}, [] {
            const auto rows = window()->findChildren<QToolButton *>("navigation-sidebar-row");
            assert(rows.size() == 1);
            assert(rows.first()->text() == QStringLiteral("Contacts"));
            assert(rows.first()->accessibleName() == QStringLiteral("Open contacts"));
            assert(rows.first()->isChecked());
            rows.first()->click();
        }},
        {{}, [] { assert(events == activation("first", "contacts")); }},
        {{scoped("SetNavigation", 1, "navigation", QJsonObject{{"items", QJsonArray{}}})}, [] {
            assert(window()->findChildren<QToolButton *>("navigation-sidebar-row").isEmpty());
        }},
    };
}

QList<Step> revisions() {
    return {
        {{surface("first", 2, "Current"), profile("first")}, [] { assert(hasTitle("Current")); }},
        {{surface("first", 1, "Stale")}, [] {
            assert(hasTitle("Current"));
            assert(!hasTitle("Stale"));
        }},
        {{surface("first", 3, "Next")}, [] {
            assert(hasTitle("Next"));
            assert(!hasTitle("Current"));
        }},
    };
}

QList<Step> values() {
    return {
        {{surface("first", 1, "First", input("Alice")), profile("first")}, [] {
            field()->setFocus();
            field()->setCursorPosition(5);
            events = {};
            QKeyEvent press(QEvent::KeyPress, Qt::Key_Z, Qt::NoModifier, "z");
            QApplication::sendEvent(field(), &press);
            assert(field()->text() == QStringLiteral("Alicez"));
        }},
        {{}, [] {
            const QJsonArray expected{
                event("SurfaceActivated", {{"surface_id", "first"}}),
                event("ValueChanged", {{"surface_id", "first"}, {"binding_id", "name"},
                    {"value", QJsonObject{{"text", "Alicez"}}}})};
            assert(events == expected);
            events = {};
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(field(), &enter);
        }},
        {{}, [] {
            assert((events == QJsonArray{event("InputSubmitted",
                {{"surface_id", "first"}, {"binding_id", "name"}})}));
            events = {};
            field()->clearFocus();
        }},
        {{}, [] {
            assert((events == QJsonArray{event("InputFocusEnded",
                {{"surface_id", "first"}, {"binding_id", "name"}})}));
        }},
    };
}

QPointer<QLineEdit> retained;

QList<Step> focus() {
    return {
        {{surface("first", 1, "First", input("Alice")), profile("first")}, [] {
            retained = field();
            field()->setFocus();
            field()->setSelection(1, 3);
        }},
        {{surface("first", 1, "Updated", input("Alice"))}, [] {
            assert(retained && retained == field());
            assert(field()->hasFocus());
            assert(field()->selectedText() == QStringLiteral("lic"));
            assert(hasTitle("Updated"));
        }},
        {{context(1, "Save")}, [] {
            assert(retained && retained == field());
            assert(field()->hasFocus());
            assert(field()->selectedText() == QStringLiteral("lic"));
            assert(button("Save") != nullptr);
        }},
    };
}

} // namespace

static void scheduleVerification() { display_test::schedule(); }
Q_COREAPP_STARTUP_FUNCTION(scheduleVerification)

int main(int argc, char **argv) {
    assert(argc == 2);
    const QString test = QString::fromUtf8(argv[1]);
    if (test == "composition") steps = composition();
    else if (test == "context") steps = contextBar();
    else if (test == "navigation") steps = navigation();
    else if (test == "revisions") steps = revisions();
    else if (test == "values") steps = values();
    else if (test == "focus") steps = focus();
    else return 2;
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    return qt_display_start(argc, argv, receive, &events);
}
