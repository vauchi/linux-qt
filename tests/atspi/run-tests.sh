#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

# Run AT-SPI tests for qvauchi under Xvfb with D-Bus + AT-SPI bus.
#
# Qt6 requires the AT-SPI bridge (QSpiAccessibleBridge) which is typically
# compiled into libQt6Gui.so. The bridge communicates with the AT-SPI
# registry daemon via D-Bus.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Verify Qt6 AT-SPI bridge is available (compiled into libQt6Gui or as plugin).
# Use grep -c to avoid SIGPIPE from grep -q with pipefail enabled.
BRIDGE_COUNT=$(nm -D /usr/lib/libQt6Gui.so.6 2>/dev/null | grep -c "QSpiAccessibleBridge" || true)
if [ "$BRIDGE_COUNT" -eq 0 ]; then
    echo "SKIP: Qt6 AT-SPI bridge (QSpiAccessibleBridge) not found in libQt6Gui — cannot run accessibility tests"
    echo "Rebuild qt6-base with AT-SPI support or install qt6-accessibility"
    exit 0
fi

# If already inside a display session with AT-SPI, run directly
if [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
    exec python3 -m pytest "$SCRIPT_DIR" "$@" -v
fi

# Otherwise, run under Xvfb with a fresh D-Bus session and AT-SPI.
# XDG_CURRENT_DESKTOP=none prevents xdg-desktop-portal from activating
# compositor-specific portals (e.g., hyprland) that crash under Xvfb.
exec env XDG_CURRENT_DESKTOP=none \
    xvfb-run -s '-screen 0 1280x720x24' \
    dbus-run-session -- bash -c "
        /usr/lib/at-spi-bus-launcher &
        sleep 0.5
        /usr/lib/at-spi2-registryd &
        sleep 0.5
        cd \"$SCRIPT_DIR\"
        python3 -m pytest . \"\$@\" -v
    " _ "$@"
