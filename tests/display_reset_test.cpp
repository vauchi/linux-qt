// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QAction>
#include <QDialog>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolTip>

using namespace display_test;

namespace {
bool hasText(const QString &text) {
    for (auto *label : window()->findChildren<QLabel *>())
        if (label->isVisible() && label->text() == text) return true;
    return false;
}

QJsonObject titledProfile(const QString &title) {
    auto command = profile("first");
    auto payload = command.value("SetPresentationProfile").toObject();
    auto value = payload.value("profile").toObject();
    value.insert("window_title", title);
    payload.insert("profile", value);
    command.insert("SetPresentationProfile", payload);
    return command;
}

QJsonObject menu(int revision, const QString &label) {
    return scoped("SetApplicationMenu", revision, "menus", QJsonArray{QJsonObject{
        {"role", "other"}, {"label", "Prepared menu"},
        {"items", QJsonArray{action(label, label)}}}});
}

QAction *menuAction(const QString &label) {
    for (auto *item : window()->findChildren<QAction *>())
        if (item->text() == label) return item;
    assert(false);
    return nullptr;
}

QJsonObject pick(const QString &title) {
    return {{"PickFile", QJsonObject{{"request_id", "pending"}, {"title", title},
        {"max_bytes", 5}, {"accepted_extensions", QJsonArray{"bin"}}}}};
}

QList<QDialog *> dialogs() {
    QList<QDialog *> result;
    for (auto *dialog : window()->findChildren<QDialog *>())
        if (dialog->isVisible()) result.append(dialog);
    return result;
}

// Feature: emergency_shred.feature — discard native presentation after Core wiped data.
QList<Step> cachedSurfaces() {
    return {{{surface("first", 9, "Private first"), surface("second", 8, "Private second"),
        titledProfile("Private window"), menu(9, "Private action")}, [] {
        assert(hasText("Private first"));
        assert(window()->windowTitle() == "Private window");
    }}, {{"ResetApplication"}, [] {
        assert(!hasText("Private first"));
        assert(!hasText("Private second"));
        assert(window()->windowTitle().isEmpty());
        for (auto *item : window()->findChildren<QAction *>())
            assert(item->text() != "Private action");
        assert(events.isEmpty());
    }}, {{profile("second")}, [] {
        assert(!hasText("Private second"));
        assert(events.isEmpty());
    }}};
}

QList<Step> trailingReset() {
    return {{{surface("first", 9, "Old"), titledProfile("Old title")}, [] {
        assert(hasText("Old"));
    }}, {{surface("first", 1, "Fresh"), titledProfile("Fresh title"),
          menu(1, "Fresh action"), "ResetApplication"}, [] {
        assert(hasText("Fresh"));
        assert(!hasText("Old"));
        assert(window()->windowTitle() == "Fresh title");
        menuAction("Fresh action")->trigger();
    }}, {{}, [] {
        assert(events == activation("first", "Fresh action"));
    }}};
}

QList<Step> pendingDialogs() {
    const auto overlay = scoped("PresentOverlay", 9, "overlay", QJsonObject{
        {"kind", "information"}, {"title", "Private overlay"}, {"body", "Private body"}});
    return {{{surface("first", 9, "Private"), profile("first"), overlay,
        pick("Private pick"), "ImagePickFromFile",
        QJsonObject{{"ExportFile", QJsonObject{{"file", QJsonObject{
            {"suggested_name", "private.bin"}, {"mime_type", "application/octet-stream"},
            {"data", QJsonArray{1}}}}}}},
        QJsonObject{{"PresentAlert", QJsonObject{{"alert", QJsonObject{
            {"title", "Private alert"}, {"message", "Private message"}}}}}},
        QJsonObject{{"ShowToast", QJsonObject{{"toast", QJsonObject{{"message", "Private toast"}}}}}}}, [] {
        assert(dialogs().size() == 5);
        assert(QToolTip::isVisible());
    }}, {{"ResetApplication"}, [] {
        assert(dialogs().isEmpty());
        assert(!QToolTip::isVisible());
        assert(events.isEmpty());
    }}, {{pick("Fresh pick")}, [] {
        const auto shown = dialogs();
        assert(shown.size() == 1);
        assert(shown.first()->windowTitle() == "Fresh pick");
        shown.first()->reject();
    }}, {{}, [] {
        assert((events == QJsonArray{event("FilePickCompleted", {
            {"request_id", "pending"}, {"outcome", "cancelled"}})}));
        assert(capabilities.size() == 1);
        assert(filePickerCapabilities.size() == 1);
    }}};
}

QList<Step> queuedObservations() {
    const QJsonArray input{QJsonObject{{"Input", QJsonObject{
        {"binding_id", "private-input"}, {"label", "Private input"},
        {"value", "Private value"}, {"input_kind", "text"}, {"enabled", true}}}}};
    return {{{surface("first", 9, "Private", input), profile("first"),
          menu(9, "Private action")}, [] {
        auto *field = window()->findChild<QLineEdit *>("private-input");
        assert(field);
        const auto reset = QJsonDocument(QJsonArray{"ResetApplication"}).toJson(QJsonDocument::Compact);
        assert(qt_display_render(reset.constData(), static_cast<size_t>(reset.size())) == 0);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Z, Qt::NoModifier, "z");
        QApplication::sendEvent(field, &press);
        menuAction("Private action")->trigger();
    }}, {{}, [] {
        assert(events.isEmpty());
        assert(window()->findChildren<QLineEdit *>().isEmpty());
        assert(!hasText("Private"));
    }}};
}

void setup() { schedule(); }
}
Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    const QString name = argc > 1 ? QString::fromUtf8(argv[1]) : "cached";
    if (name == "cached") steps = cachedSurfaces();
    else if (name == "trailing") steps = trailingReset();
    else if (name == "dialogs") steps = pendingDialogs();
    else if (name == "queued") steps = queuedObservations();
    else return 2;
    const int result = qt_display_start(argc, argv, receive, &events);
    assert(result == 0);
    return result;
}
