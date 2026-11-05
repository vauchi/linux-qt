// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qt_display.h"
#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QTimer>
#include <cassert>
#include <functional>
#include <utility>

namespace display_test {

struct Step {
    QJsonArray commands;
    std::function<void()> verify;
    std::function<bool()> ready;
};

inline QList<Step> steps;
inline QJsonArray events;
inline QJsonArray capabilities;
inline QJsonArray filePickerCapabilities;
inline bool stopAfterLastStep = true;

inline QJsonObject action(const QString &id, const QString &label) {
    return {{"interaction_id", id}, {"label", label},
            {"accessibility_label", label}, {"enabled", true}};
}

inline QJsonObject surface(const QString &id, int revision, const QString &title,
                           const QJsonArray &nodes = {}) {
    return {{"ReplaceSurface", QJsonObject{{"surface", QJsonObject{
        {"surface_id", id}, {"revision", revision}, {"title", title},
        {"layout", "fixed"}, {"tokens", QJsonObject{}}, {"nodes", nodes}}}}}};
}

inline QJsonObject profile(const QString &active, bool split = false) {
    return {{"SetPresentationProfile", QJsonObject{{"profile", QJsonObject{
        {"window_class", "expanded"}, {"pane_layout", split ? "split" : "single"},
        {"primary_surface", "first"}, {"detail_surface", "second"},
        {"active_surface", active}}}}}};
}

inline QJsonObject scoped(const QString &command, int revision,
                          const QString &key, const QJsonValue &value,
                          const QString &id = QStringLiteral("first")) {
    return {{command, QJsonObject{{"surface_id", id}, {"revision", revision},
                                  {key, value}}}};
}

inline QMainWindow *window() {
    QList<QMainWindow *> windows;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *main = qobject_cast<QMainWindow *>(widget);
            main && main->isVisible()) windows.append(main);
    }
    assert(windows.size() == 1);
    return windows.first();
}

inline QJsonObject event(const QString &variant, const QJsonObject &payload) {
    return {{variant, payload}};
}

inline QJsonArray activation(const QString &id, const QString &interaction) {
    return {event("SurfaceActivated", {{"surface_id", id}}),
            event("ActionActivated", {{"surface_id", id}, {"interaction_id", interaction}})};
}

inline void receive(const char *json, void *context) {
    assert(context == &events);
    const auto value = QJsonDocument::fromJson(json).object();
    if (value.contains("ApplicationMenuAvailabilityChanged")) capabilities.append(value);
    else if (value.contains("FilePickerCapabilitiesChanged")) filePickerCapabilities.append(value);
    else events.append(value);
}

class ReadyWatcher : public QObject {
public:
    ReadyWatcher(std::function<bool()> ready, std::function<void()> finish)
        : QObject(qApp), m_ready(std::move(ready)), m_finish(std::move(finish)) {
        qApp->installEventFilter(this);
        queueCheck();
    }
protected:
    bool eventFilter(QObject *receiver, QEvent *) override {
        if (receiver != this) queueCheck();
        return false;
    }
private:
    void queueCheck() {
        if (m_scheduled) return;
        m_scheduled = true;
        QMetaObject::invokeMethod(this, [this] {
            m_scheduled = false;
            if (!m_ready()) return;
            qApp->removeEventFilter(this);
            deleteLater();
            m_finish();
        }, Qt::QueuedConnection);
    }
    std::function<bool()> m_ready;
    std::function<void()> m_finish;
    bool m_scheduled = false;
};

// @internal
inline void runStep(int index) {
    const auto step = steps.at(index);
    const auto json = QJsonDocument(step.commands).toJson(QJsonDocument::Compact);
    assert(qt_display_render(json.constData(), static_cast<size_t>(json.size())) == 0);
    QMetaObject::invokeMethod(qApp, [step, index] {
        const auto finish = [step, index] {
            step.verify();
            if (index + 1 < steps.size()) runStep(index + 1);
            else if (stopAfterLastStep) qt_display_stop();
        };
        if (!step.ready || step.ready()) finish();
        else new ReadyWatcher(step.ready, finish);
    }, Qt::QueuedConnection);
}

inline void schedule() {
    QTimer::singleShot(0, [] { runStep(0); });
}

} // namespace display_test
