// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// Core may send a display code's `placement`: its side and the offset of
// its top-left corner, in permille of the node's square, and the
// `error_correction` level to draw it at. Absent placement means the code
// fills the square; absent level means medium. The shell draws where it is
// told and never outside its own square (vauchi/private#450, as on
// iOS/macOS: macos/VauchiTests/CoreUI/PresentationQrPlacementTests.swift).
//
// Pure functions, so the numbers are asserted without rendering anything.

#include "coreui/presentationsurface_qr.h"

#include <QJsonObject>
#include <cassert>

namespace {

QJsonObject placementJson(int size, int x, int y) {
    return {{"size", size}, {"x", x}, {"y", y}};
}

} // namespace

int main() {
    constexpr int squareSide = 200;

    {
        // An absent placement leaves the code filling its square.
        const vauchi::QrFrameSpec frame =
            vauchi::qrFrameSpec(QJsonValue(), squareSide);
        assert((frame == vauchi::QrFrameSpec{squareSide, 0, 0}));
    }

    {
        // A placed code is scaled and offset within the square.
        const vauchi::QrFrameSpec frame = vauchi::qrFrameSpec(
            placementJson(650, 350, 175), squareSide);
        assert((frame == vauchi::QrFrameSpec{130, 70, 35}));
    }
    {
        const vauchi::QrFrameSpec frame =
            vauchi::qrFrameSpec(placementJson(800, 200, 0), squareSide);
        assert((frame == vauchi::QrFrameSpec{160, 40, 0}));
    }

    {
        // Core never sends these; a shell still must not draw past its
        // node.
        const vauchi::QrFrameSpec frame =
            vauchi::qrFrameSpec(placementJson(800, 900, 5000), squareSide);
        assert((frame == vauchi::QrFrameSpec{160, 40, 40}));
    }
    {
        const vauchi::QrFrameSpec frame =
            vauchi::qrFrameSpec(placementJson(4000, 10, 10), squareSide);
        assert((frame == vauchi::QrFrameSpec{squareSide, 0, 0}));
    }
    {
        const vauchi::QrFrameSpec frame =
            vauchi::qrFrameSpec(placementJson(500, -20, -1), squareSide);
        assert((frame == vauchi::QrFrameSpec{100, 0, 0}));
    }

    {
        // A nonsensical size falls back to the full square.
        const vauchi::QrFrameSpec frame =
            vauchi::qrFrameSpec(placementJson(0, 0, 0), squareSide);
        assert((frame == vauchi::QrFrameSpec{squareSide, 0, 0}));
    }
    {
        const vauchi::QrFrameSpec frame =
            vauchi::qrFrameSpec(placementJson(-5, 0, 0), squareSide);
        assert((frame == vauchi::QrFrameSpec{squareSide, 0, 0}));
    }

    // A low error-correction level is drawn as such; absent or
    // unrecognised draws at medium, what shells drew before.
    assert(vauchi::qrCorrectionLevel(QJsonValue("low")) == QR_ECLEVEL_L);
    assert(vauchi::qrCorrectionLevel(QJsonValue()) == QR_ECLEVEL_M);
    assert(vauchi::qrCorrectionLevel(QJsonValue("medium")) == QR_ECLEVEL_M);
    assert(vauchi::qrCorrectionLevel(QJsonValue("ultra")) == QR_ECLEVEL_M);

    return 0;
}
