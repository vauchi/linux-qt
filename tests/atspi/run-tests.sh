#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

# Run AT-SPI tests for qvauchi under Xvfb with D-Bus + AT-SPI bus.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
    exec python3 -m pytest "$SCRIPT_DIR" "$@" -v
fi

exec xvfb-run -s '-screen 0 1280x720x24' \
    dbus-run-session -- bash -c "
        /usr/lib/at-spi-bus-launcher &
        sleep 0.5
        /usr/lib/at-spi2-registryd &
        sleep 0.5
        cd \"$SCRIPT_DIR\"
        python3 -m pytest . \"\$@\" -v
    " _ "$@"
