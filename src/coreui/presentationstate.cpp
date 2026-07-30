// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presentationstate.h"

bool PresentationState::apply(const QJsonObject &command) {
    if (command.contains(QStringLiteral("ReplaceSurface"))) {
        const QJsonObject surface =
            command.value(QStringLiteral("ReplaceSurface")).toObject()
                .value(QStringLiteral("surface")).toObject();
        const QString id = surface.value(QStringLiteral("surface_id")).toString();
        const qint64 revision =
            surface.value(QStringLiteral("revision")).toInteger(-1);
        if (id.isEmpty() || revision < 0) {
            return false;
        }
        const auto current = m_surfaces.constFind(id);
        if (current != m_surfaces.cend()
            && revision
                   < current->value(QStringLiteral("revision")).toInteger()) {
            return false;
        }
        m_surfaces.insert(id, surface);
        m_contextBars.remove(id);
        m_overlays.remove(id);
        m_lastSurface = id;
        return true;
    }
    if (command.contains(QStringLiteral("SetContextBar"))) {
        const QJsonObject payload =
            command.value(QStringLiteral("SetContextBar")).toObject();
        const QString id = payload.value(QStringLiteral("surface_id")).toString();
        const qint64 revision =
            payload.value(QStringLiteral("revision")).toInteger(-1);
        if (!isCurrentRevision(id, revision)) {
            return false;
        }
        m_contextBars.insert(id, payload.value(QStringLiteral("bar")).toObject());
        return true;
    }
    if (command.contains(QStringLiteral("SetPresentationProfile"))) {
        m_profile =
            command.value(QStringLiteral("SetPresentationProfile")).toObject()
                .value(QStringLiteral("profile")).toObject();
        return !m_profile.isEmpty();
    }
    if (command.contains(QStringLiteral("PresentOverlay"))) {
        const QJsonObject payload =
            command.value(QStringLiteral("PresentOverlay")).toObject();
        const QString id = payload.value(QStringLiteral("surface_id")).toString();
        const qint64 revision =
            payload.value(QStringLiteral("revision")).toInteger(-1);
        if (!isCurrentRevision(id, revision)) {
            return false;
        }
        m_overlays.insert(
            id, payload.value(QStringLiteral("overlay")).toObject());
        return true;
    }
    return true;
}

std::optional<QJsonObject>
PresentationState::surface(const QString &surfaceId) const {
    const auto found = m_surfaces.constFind(surfaceId);
    if (found == m_surfaces.cend()) {
        return std::nullopt;
    }
    return *found;
}

QStringList PresentationState::visibleSurfaceIds() const {
    if (m_profile.isEmpty()) {
        return m_lastSurface.isEmpty() ? QStringList{} : QStringList{m_lastSurface};
    }
    const QString primary =
        m_profile.value(QStringLiteral("primary_surface")).toString();
    const QString detail =
        m_profile.value(QStringLiteral("detail_surface")).toString();
    if (m_profile.value(QStringLiteral("pane_layout")).toString()
        == QStringLiteral("split")) {
        QStringList result;
        if (m_surfaces.contains(primary)) {
            result.append(primary);
        }
        if (!detail.isEmpty() && m_surfaces.contains(detail)) {
            result.append(detail);
        }
        return result;
    }
    const QString active = activeSurfaceId();
    return active.isEmpty() ? QStringList{} : QStringList{active};
}

QJsonObject PresentationState::contextBar() const {
    return m_contextBars.value(activeSurfaceId());
}

std::optional<QJsonObject> PresentationState::overlay() const {
    const QString active = activeSurfaceId();
    const auto found = m_overlays.constFind(active);
    if (found == m_overlays.cend()) {
        return std::nullopt;
    }
    return *found;
}

QJsonObject PresentationState::profile() const {
    return m_profile;
}

bool PresentationState::isCurrentRevision(const QString &surfaceId,
                                          qint64 revision) const {
    const auto found = m_surfaces.constFind(surfaceId);
    return found != m_surfaces.cend()
           && found->value(QStringLiteral("revision")).toInteger() == revision;
}

QString PresentationState::activeSurfaceId() const {
    const QString active =
        m_profile.value(QStringLiteral("active_surface")).toString();
    return m_surfaces.contains(active) ? active : m_lastSurface;
}
