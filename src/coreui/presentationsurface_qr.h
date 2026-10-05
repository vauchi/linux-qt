// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonValue>
#include <qrencode.h>

namespace vauchi {

/// A display code's side and top-left corner inside its node's square,
/// in pixels.
struct QrFrameSpec {
    int side = 0;
    int left = 0;
    int top = 0;

    bool operator==(const QrFrameSpec &other) const {
        return side == other.side && left == other.left
            && top == other.top;
    }
};

/// Where Core's optional `placement` (`{size, x, y}` in permille of the
/// node's square) puts a display code, in pixels. No placement, or a
/// non-positive size, is the full square at (0, 0). Core only ever sends
/// placements inside the square; a value outside it is still pulled back
/// in, so the shell never draws past its own node (vauchi/private#450,
/// as on iOS/macOS).
QrFrameSpec qrFrameSpec(const QJsonValue &placement, int squareSide);

/// Core's `error_correction` name to qrencode's level. Absent or
/// unrecognised draws at medium, what shells drew before
/// (vauchi/private#450).
QRecLevel qrCorrectionLevel(const QJsonValue &errorCorrection);

} // namespace vauchi
