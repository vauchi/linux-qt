// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

namespace vauchi {

/// Freedesktop icon names to try, in order, for the platform-neutral
/// `icon_token` Core attaches to a navigation item. Never empty: a token this
/// build has not learned yields the neutral fallback chain. A
/// `pictogram.<group>.<name>` token yields the bundled pictogram's resource
/// path (`:/pictograms/<group>/<name>.svg`) ahead of that chain.
QStringList navigationIconNames(const QString &token);

/// The first candidate from `navigationIconNames` that can actually be drawn
/// — a bundled pictogram, tinted with the palette's text colour, or a name
/// the active icon theme carries — ending at a style-supplied icon so a
/// themeless session still gets a glyph rather than an empty gap. Never null.
QIcon navigationIcon(const QString &token);

/// The bundled pictogram, tinted like `navigationIcon`'s, that a list row or
/// status node draws for `token`. Null for anything but a pictogram this
/// build can draw: those surfaces had no icon before pictograms existed, so
/// no fallback marker is added.
QIcon contentPictogram(const QString &token);

/// Edge length, in device-independent pixels, of a content pictogram.
constexpr int kPictogramSize = 24;

} // namespace vauchi
