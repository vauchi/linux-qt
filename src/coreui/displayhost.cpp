// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include "applicationmenubar.h"
#include "navigationsidebar.h"
#include "presentationsurface.h"
#include "../platform/filepicker.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSplitter>
#include <algorithm>

QtDisplayHost::QtDisplayHost(QtDisplayEventCallback onEvent, void *userData)
    : m_onEvent(onEvent), m_userData(userData),
      m_row(new QSplitter(Qt::Horizontal, this)),
      m_panes(new QSplitter(Qt::Horizontal, m_row)) {
    setCentralWidget(m_row);
    resetFileEffects();
    resetApplicationMenu();
    resize(900, 700);
}

void QtDisplayHost::render(const QJsonArray &commands) {
    const bool reset = std::any_of(commands.begin(), commands.end(), [](const auto &value) {
        return value.toString() == "ResetApplication";
    });
    // Core appends ResetApplication after the replacement surface (#1073).
    if (reset) resetPresentation();
    bool changed = reset;
    for (const auto &value : commands) {
        if (value.toString() == "PerformNativeBack") {
            QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
            continue;
        }
        if (pickLegacyFile(value)) continue;
        const auto command = value.toObject();
        // Owner decision #1073: prepared QR nodes are authoritative; one-shot output is terminal-only.
        if (command.contains("QrDisplay") || command.contains("EmitDocument")
            || command.contains("FinishInvocation")) continue;
        if (applyNativeEffect(command)) continue;
        const QString transport = command.contains("SetScreenBrightness") ? "screen_brightness"
            : command.contains("SetIdleTimerDisabled") ? "idle_timer"
            : command.contains("SetOrientationLock") ? "orientation_lock" : QString{};
        if (!transport.isEmpty()) {
            emitEvent({{"HardwareUnavailable", QJsonObject{{"transport", transport}}}});
            continue;
        }
        if (command.contains("ExportFile")) {
            exportFile(command);
            continue;
        }
        if (command.contains("PickFile")) {
            m_filePicker->pick(command.value("PickFile").toObject());
            continue;
        }
        if (command.contains("CancelFilePick")) {
            m_filePicker->cancel(command.value("CancelFilePick").toObject().value("request_id").toString());
            continue;
        }
        if (command.contains("SetApplicationMenu")) {
            applyApplicationMenu(command.value("SetApplicationMenu").toObject());
            continue;
        }
        if (command.contains("PresentOverlay") || command.contains("DismissOverlay")) {
            applyOverlay(command);
            continue;
        }
        const bool structural = command.contains("ReplaceSurface")
            || command.contains("SetPresentationProfile")
            || command.contains("SetContextBar")
            || command.contains("SetNavigation");
        if (!structural || !m_state.apply(command)) continue;
        changed = true;
        if (command.contains("ReplaceSurface")) {
            const auto id = command.value("ReplaceSurface").toObject()
                                .value("surface").toObject().value("surface_id").toString();
            if (id == m_overlaySurface) closeOverlay();
            if (id == m_menuSurface && m_state.surface(id)->value("revision").toInteger() != m_menuRevision) {
                m_applicationMenu->apply({});
                m_menuSurface.clear();
                m_menuRevision = -1;
            }
        }
        if (command.contains("SetPresentationProfile")) {
            const auto title = m_state.profile().value("window_title");
            if (title.isString()) setWindowTitle(title.toString());
        }
    }
    if (changed) renderPresentation();
    snapshotIfAsked();
}

void QtDisplayHost::reportCapabilities() {
    emitEvent({{"ApplicationMenuAvailabilityChanged", QJsonObject{{"available", true}}}});
    emitEvent({{"FilePickerCapabilitiesChanged", QJsonObject{{"correlated_results", true}}}});
}

void QtDisplayHost::applyApplicationMenu(const QJsonObject &payload) {
    const auto id = payload.value("surface_id").toString();
    const auto surface = m_state.surface(id);
    const auto revision = payload.value("revision").toInteger(-1);
    if (!surface || revision != surface->value("revision").toInteger()) return;
    m_applicationMenu->apply(payload);
    m_menuSurface = id;
    m_menuRevision = revision;
}

void QtDisplayHost::renderPresentation() {
    const auto visible = m_state.visibleSurfaceIds();
    const auto active = m_state.activeSurfaceId();
    const auto navigation = m_state.navigation();
    const bool sidebarShown = !navigation.value("items").toArray().isEmpty();
    if (navigation != m_navigation || active != m_navigationSurface) {
        delete m_sidebar;
        if (sidebarShown) {
            m_sidebar = new NavigationSidebar(navigation);
            connect(m_sidebar, &NavigationSidebar::interactionReady, this,
                [this, active, generation = m_generation](const QString &id) {
                    if (generation == m_generation) activate(active, id);
                },
                Qt::QueuedConnection);
            m_row->insertWidget(0, m_sidebar);
            m_row->setStretchFactor(0, 0);
            m_row->setStretchFactor(1, 1);
        }
        m_navigation = navigation;
        m_navigationSurface = active;
    }
    for (auto item = m_surfaces.begin(); item != m_surfaces.end();) {
        if (!visible.contains(item.key())) {
            delete item.value();
            item = m_surfaces.erase(item);
        } else ++item;
    }
    for (qsizetype index = 0; index < visible.size(); ++index) {
        const auto id = visible.at(index);
        const auto surface = m_state.surface(id);
        if (!surface) continue;
        const auto bar = id == active ? m_state.contextBar() : QJsonObject{};
        auto *widget = m_surfaces.value(id).data();
        if (!widget || !widget->update(*surface, bar, sidebarShown)) {
            delete widget;
            widget = createSurface(*surface, bar, sidebarShown);
            m_surfaces.insert(id, widget);
        }
        if (m_panes->widget(static_cast<int>(index)) != widget)
            m_panes->insertWidget(static_cast<int>(index), widget);
        widget->show();
    }
}

PresentationSurface *QtDisplayHost::createSurface(const QJsonObject &surface,
                                                 const QJsonObject &bar,
                                                 bool sidebar) {
    auto *widget = new PresentationSurface(surface, bar, sidebar);
    const auto generation = m_generation;
    connect(widget, &PresentationSurface::interactionReady, this,
        [this, generation](const QString &id, const QString &interaction) {
            if (generation == m_generation) activate(id, interaction);
        }, Qt::QueuedConnection);
    connect(widget, &PresentationSurface::valueReady, this,
        [this, generation](const QString &id, const QString &binding, const QJsonValue &value) {
            if (generation != m_generation) return;
            emitEvent({{"SurfaceActivated", QJsonObject{{"surface_id", id}}}});
            emitEvent({{"ValueChanged", QJsonObject{{"surface_id", id},
                {"binding_id", binding}, {"value", value}}}});
        }, Qt::QueuedConnection);
    connect(widget, &PresentationSurface::submitReady, this,
        [this, generation](const QString &id, const QString &binding) {
            if (generation != m_generation) return;
            emitEvent({{"InputSubmitted", QJsonObject{{"surface_id", id},
                {"binding_id", binding}}}});
        }, Qt::QueuedConnection);
    connect(widget, &PresentationSurface::focusEndReady, this,
        [this, generation](const QString &id, const QString &binding) {
            if (generation != m_generation) return;
            emitEvent({{"InputFocusEnded", QJsonObject{{"surface_id", id},
                {"binding_id", binding}}}});
        }, Qt::QueuedConnection);
    return widget;
}

void QtDisplayHost::activate(const QString &surfaceId, const QString &interactionId) {
    emitEvent({{"SurfaceActivated", QJsonObject{{"surface_id", surfaceId}}}});
    emitEvent({{"ActionActivated", QJsonObject{{"surface_id", surfaceId},
        {"interaction_id", interactionId}}}});
}

void QtDisplayHost::emitEvent(const QJsonObject &event) {
    if (!m_onEvent) return;
    const auto json = QJsonDocument(event).toJson(QJsonDocument::Compact);
    m_onEvent(json.constData(), m_userData);
}

void QtDisplayHost::snapshotIfAsked() {
    const auto path = qgetenv("QT_DISPLAY_SNAPSHOT");
    if (path.isEmpty()) return;
    QCoreApplication::processEvents();
    grab().save(QString::fromUtf8(path));
}
