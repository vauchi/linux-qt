// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>

/// `--render-catalog <catalog.json> <out-dir> [width] [height]` replays
/// every screen of Core's screen catalog through the production
/// PresentationController (surface, context bar, navigation sidebar) inside
/// a QMainWindow and grabs one PNG per screen and variant into `out-dir`:
/// `<code_id>.png` (default dark theme), `<code_id>.light.png`, and
/// `<code_id>.large.png` (UI font scaled 1.5x). Headless under the Qt
/// `offscreen` platform. Returns the process exit code; a negative result
/// means "not in render-catalog mode".
int maybeRenderCatalog(const QStringList &args);
