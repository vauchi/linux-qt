// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include <optional>

/// Humble Qt projection of Core's canonical presentation command state.
class PresentationState {
public:
    bool apply(const QJsonObject &command);

    std::optional<QJsonObject> surface(const QString &surfaceId) const;
    QStringList visibleSurfaceIds() const;
    QJsonObject contextBar() const;
    std::optional<QJsonObject> overlay() const;
    QJsonObject profile() const;
    QString activeSurfaceId() const;

private:
    bool isCurrentRevision(const QString &surfaceId, qint64 revision) const;

    QHash<QString, QJsonObject> m_surfaces;
    QHash<QString, QJsonObject> m_contextBars;
    QHash<QString, QJsonObject> m_overlays;
    QJsonObject m_profile;
    QString m_lastSurface;
};
