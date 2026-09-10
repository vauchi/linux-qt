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

} // namespace vauchi
