// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QWidget>

namespace vauchi {

/// One row of Core's per-surface `SetNavigation` destination list.
struct NavigationRow {
    QString interactionId;
    QString label;
    QString accessibilityLabel;
    QString iconToken;
    bool selected = false;
    int badgeCount = 0;
};

/// Parses the `navigation.items` array `SetNavigation` carries. A field an
/// item omits yields that field's default (empty string, false, or zero)
/// rather than dropping the row — Core is the sole validator of this
/// payload (ADR-066); the shell only maps it to widgets.
QList<NavigationRow> navigationRows(const QJsonObject &navigation);

/// Index of the row Core marked `selected`, or -1 when none is (an empty
/// list included).
int selectedNavigationRow(const QList<NavigationRow> &rows);

} // namespace vauchi

/// Persistent destination column rendered left of the active surface in a
/// QSplitter. One focusable row per `vauchi::NavigationRow`; an empty
/// `navigation.items` renders no rows, which the caller uses to keep the
/// splitter's sidebar pane out of the layout entirely — a locked app
/// publishes an empty list rather than a dedicated "no navigation" command.
class NavigationSidebar : public QWidget {
    Q_OBJECT

public:
    explicit NavigationSidebar(const QJsonObject &navigation,
                               QWidget *parent = nullptr);

signals:
    /// Reuses the navigation overlay's activation path: the caller pairs
    /// this with the active surface id and dispatches the same
    /// SurfaceActivated + ActionActivated sequence.
    void interactionReady(const QString &interactionId);
};
