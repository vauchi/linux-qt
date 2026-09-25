// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>

namespace vauchi {

/// Renders one avatar as a `diameter` x `diameter` pixmap. With image data,
/// the image is centre-cropped to fill the canvas and, when `circular`,
/// clipped to a circle — the `shape: Circle` mask no shell-side `setPixmap`
/// call applied before. With no image data, paints a `fillColor`-filled
/// circle (or `cornerRadius`-rounded rect when not `circular`) with
/// `fallbackText` centred in `textColor`, so the initials always have a
/// ground instead of sitting bare on the window background.
///
/// Returns a null pixmap for `diameter <= 0`.
QPixmap avatarPixmap(const QPixmap &imageData, const QString &fallbackText,
                     bool circular, int diameter, int cornerRadius,
                     const QColor &fillColor, const QColor &textColor);

/// Renders one `Image` node's picture (or fallback text) fit inside a
/// `size` x `size` canvas: aspect kept, letterboxed rather than cropped,
/// and centred. The opposite trade-off from `avatarPixmap()`, which crops
/// to fill — this is for Core's `size` hint (e.g. the onboarding mark),
/// never for the avatar case that hint leaves untouched.
///
/// Returns a null pixmap for `size <= 0`.
QPixmap fittedImagePixmap(const QPixmap &imageData, const QString &fallbackText,
                          int size, const QColor &textColor);

} // namespace vauchi
