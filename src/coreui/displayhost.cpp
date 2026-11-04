// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include "navigationsidebar.h"
#include "presentationsurface.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSplitter>

QtDisplayHost::QtDisplayHost(QtDisplayEventCallback onEvent, void *userData)
    : m_onEvent(onEvent), m_userData(userData),
      m_row(new QSplitter(Qt::Horizontal, this)),
      m_panes(new QSplitter(Qt::Horizontal, m_row)) {
    setCentralWidget(m_row);
    resize(900, 700);
}

void QtDisplayHost::render(const QJsonArray &commands) {
    bool changed = false;
    for (const auto &value : commands) {
        const auto command = value.toObject();
        const bool structural = command.contains("ReplaceSurface")
            || command.contains("SetPresentationProfile")
            || command.contains("SetContextBar")
            || command.contains("SetNavigation");
        if (!structural || !m_state.apply(command)) continue;
        changed = true;
        if (command.contains("SetPresentationProfile")) {
            const auto title = m_state.profile().value("window_title");
            if (title.isString()) setWindowTitle(title.toString());
        }
    }
    if (changed) renderPresentation();
    snapshotIfAsked();
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
                [this, active](const QString &id) { activate(active, id); },
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
    connect(widget, &PresentationSurface::interactionReady, this,
        &QtDisplayHost::activate, Qt::QueuedConnection);
    connect(widget, &PresentationSurface::valueReady, this,
        [this](const QString &id, const QString &binding, const QJsonValue &value) {
            emitEvent({{"SurfaceActivated", QJsonObject{{"surface_id", id}}}});
            emitEvent({{"ValueChanged", QJsonObject{{"surface_id", id},
                {"binding_id", binding}, {"value", value}}}});
        }, Qt::QueuedConnection);
    connect(widget, &PresentationSurface::submitReady, this,
        [this](const QString &id, const QString &binding) {
            emitEvent({{"InputSubmitted", QJsonObject{{"surface_id", id},
                {"binding_id", binding}}}});
        }, Qt::QueuedConnection);
    connect(widget, &PresentationSurface::focusEndReady, this,
        [this](const QString &id, const QString &binding) {
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
