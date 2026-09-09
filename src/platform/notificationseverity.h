// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Maps Core's generic notification urgency hint onto the tray message
/// severity. Core decides urgency; the shell only translates it (ADR-066).

#pragma once

#include <QString>
#include <QSystemTrayIcon>

namespace vauchi {

inline QSystemTrayIcon::MessageIcon trayIconForPriority(const QString &priority) {
    if (priority == QLatin1String("Urgent")) return QSystemTrayIcon::Critical;
    if (priority == QLatin1String("High")) return QSystemTrayIcon::Warning;
    return QSystemTrayIcon::Information;
}

} // namespace vauchi
