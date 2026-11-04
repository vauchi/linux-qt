// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_display.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QTimer>
#include <cassert>

namespace {

struct TitleCase {
    QJsonArray commands;
    QString expected;
};

QJsonObject profile(const QJsonObject &value) {
    QJsonObject prepared{{"window_class", "compact"}, {"pane_layout", "single"},
                         {"primary_surface", "fixture"}, {"active_surface", "fixture"},
                         {"detail_surface", QJsonValue::Null}};
    for (auto item = value.constBegin(); item != value.constEnd(); ++item) {
        prepared.insert(item.key(), item.value());
    }
    return {{"SetPresentationProfile", QJsonObject{{"profile", prepared}}}};
}

QJsonObject surface(int revision) {
    return {{"ReplaceSurface", QJsonObject{{"surface", QJsonObject{
        {"surface_id", "fixture"}, {"revision", revision},
        {"title", "Surface title"}, {"tokens", QJsonObject{}},
        {"nodes", QJsonArray{}}}}}}};
}

const QList<TitleCase> &cases() {
    static const QList<TitleCase> values{
        {{profile({{"window_title", "Prepared window"}}), surface(1)}, "Prepared window"},
        {{surface(2)}, "Prepared window"},
        {{profile({})}, "Prepared window"},
        {{profile({{"window_title", QJsonValue::Null}})}, "Prepared window"},
        {{profile({{"window_title", ""}})}, ""},
    };
    return values;
}

// @internal
void verifyCase(int index) {
    const auto test = cases().at(index);
    const auto commands = QJsonDocument(test.commands).toJson(QJsonDocument::Compact);
    assert(qt_display_render(commands.constData(), static_cast<size_t>(commands.size())) == 0);
    QMetaObject::invokeMethod(qApp, [index, test] {
        QList<QMainWindow *> windows;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *window = qobject_cast<QMainWindow *>(widget);
                window && window->isVisible()) windows.append(window);
        }
        assert(windows.size() == 1);
        assert(windows.first()->windowTitle() == test.expected);
        if (index + 1 == cases().size()) qt_display_stop();
        else verifyCase(index + 1);
    }, Qt::QueuedConnection);
}

void scheduleVerification() {
    QTimer::singleShot(0, [] { verifyCase(0); });
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(scheduleVerification)

int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    const int result = qt_display_start(argc, argv, nullptr, nullptr);
    assert(result == 0);
    return result;
}
