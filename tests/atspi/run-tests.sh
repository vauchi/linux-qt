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

# Locate libQt6Gui before probing it. The path is multiarch on Debian
# (/lib/x86_64-linux-gnu/...), and this used to look only at the literal
# /usr/lib/libQt6Gui.so.6 — which exists on no Debian machine. nm failed, its
# error went to /dev/null, grep counted zero, and the suite skipped itself
# green from 2026-03-18 until 2026-09-08 on a runner that had the bridge all
# along (22 symbols).
resolve_qt_gui_lib() {
    ldconfig_bin=$(command -v ldconfig || echo /sbin/ldconfig)
    if [ -x "$ldconfig_bin" ]; then
        lib=$("$ldconfig_bin" -p 2>/dev/null \
            | awk '/libQt6Gui\.so\.6/ { print $NF; exit }')
        if [ -n "$lib" ] && [ -e "$lib" ]; then
            printf '%s\n' "$lib"
            return 0
        fi
    fi
    for candidate in /usr/lib/*/libQt6Gui.so.6 /lib/*/libQt6Gui.so.6 \
        /usr/lib/libQt6Gui.so.6; do
        if [ -e "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

QT_GUI_LIB=$(resolve_qt_gui_lib || true)

# Use grep -c to avoid SIGPIPE from grep -q with pipefail enabled.
BRIDGE_COUNT=0
if [ -n "$QT_GUI_LIB" ]; then
    BRIDGE_COUNT=$(nm -D "$QT_GUI_LIB" 2>/dev/null \
        | grep -c "QSpiAccessibleBridge" || true)
fi

if [ "$BRIDGE_COUNT" -eq 0 ]; then
    if [ -z "$QT_GUI_LIB" ]; then
        echo "Qt6 AT-SPI bridge: libQt6Gui.so.6 not found on this system"
    else
        echo "Qt6 AT-SPI bridge (QSpiAccessibleBridge) not found in $QT_GUI_LIB"
    fi
    echo "Rebuild qt6-base with AT-SPI support or install qt6-accessibility"
    # A developer without the bridge wants a skip. CI must not report success
    # for a suite it did not run — that is how the six-month gap above stayed
    # invisible.
    if [ -n "${CI:-}" ]; then
        echo "ERROR: refusing to report success in CI without running the suite" >&2
        exit 1
    fi
    echo "SKIP: cannot run accessibility tests"
    exit 0
fi

echo "Qt6 AT-SPI bridge: $BRIDGE_COUNT symbols in $QT_GUI_LIB"

# If already inside a display session with AT-SPI, run directly
if [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
    exec python3 -m pytest "$SCRIPT_DIR" "$@" -v
fi

# Otherwise, run under Xvfb with a fresh D-Bus session and AT-SPI.
# XDG_CURRENT_DESKTOP=none prevents xdg-desktop-portal from activating
# compositor-specific portals (e.g., hyprland) that crash under Xvfb.
# QT_QPA_PLATFORM=xcb forces X11 backend (Qt defaults to wayland which crashes under Xvfb)
LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"
UPDATE_SNAPSHOTS="${UPDATE_SNAPSHOTS:-}"
export LD_LIBRARY_PATH UPDATE_SNAPSHOTS

exec env XDG_CURRENT_DESKTOP=none QT_QPA_PLATFORM=xcb \
    xvfb-run -a -s '-screen 0 1280x720x24' \
    dbus-run-session -- bash -c "
        /usr/lib/at-spi-bus-launcher &
        sleep 0.5
        /usr/lib/at-spi2-registryd &
        sleep 0.5
        cd \"$SCRIPT_DIR\"
        python3 -m pytest . \"\$@\" -v
    " _ "$@"
