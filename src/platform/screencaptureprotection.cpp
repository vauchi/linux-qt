// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "screencaptureprotection.h"

#include <QDebug>

void enableScreenCaptureProtection() {
    // Intentional no-op. Surfaces the platform gap at startup so
    // dev builds make it obvious the protection is *not* active —
    // helps prevent the false sense of security from "we call
    // protect everywhere".
    qInfo() << "[vauchi] screen-capture protection: not enforceable on Linux/Qt6 "
               "(compositor-mediated). See investigation doc.";
}
