// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

namespace vauchi {

/// Freedesktop icon names to try, in order, for the platform-neutral
/// `icon_token` Core attaches to a navigation item. Never empty: a token this
/// build has not learned yields the neutral fallback chain.
QStringList navigationIconNames(const QString &token);

/// The first name from `navigationIconNames` the active icon theme can
/// actually draw, ending at a style-supplied icon so a themeless session
/// still gets a glyph rather than an empty gap. Never null.
QIcon navigationIcon(const QString &token);

} // namespace vauchi
