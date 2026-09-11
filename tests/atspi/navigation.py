# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""AT-SPI navigation helpers for qvauchi.

Core drives navigation through the context bar's launcher button, which
opens an overlay listing the destinations, and (since the persistent
navigation landed) through sidebar rows. Both are driven here by their
AT-SPI actions.
"""

import time

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402

from helpers import click_button, find_all, find_app, wait_for_element

# Core labels the context bar's navigation launcher "More" in the test
# locale (see test_contextual_surface.py).
NAVIGATION_LAUNCHER = "More"
EXPECTED_DESTINATIONS = ["Contacts", "My Card", "Exchange", "Devices", "Settings"]
ESCAPE_KEYCODE = 9
ACTIONABLE_ROLES = {"push button", "button", "check box", "toggle button", "radio button"}


def rebind(pid: int):
    """Re-resolve the app root for ``pid``.

    Qt re-registers its AT-SPI tree across surface swaps, so a root handle
    taken before an interaction reports an empty tree afterwards
    (test_reliability.py re-finds the app after every click for the same
    reason). Every helper here re-resolves before reading the tree.
    """
    return find_app("vauchi", timeout=5.0, pid=pid)


def press_escape():
    Atspi.generate_keyboard_event(ESCAPE_KEYCODE, "", Atspi.KeySynthType.PRESSRELEASE)
    time.sleep(0.2)


def buttons(root):
    nodes = find_all(root, role="push button")
    nodes.extend(find_all(root, role="button"))
    return nodes


def _open_overlay(app):
    if not click_button(app, NAVIGATION_LAUNCHER, timeout=3.0):
        return None
    return wait_for_element(app, role="dialog", timeout=3.0)


def overlay_destinations(pid: int) -> list[str]:
    """Destination labels listed by Core's navigation overlay."""
    app = rebind(pid)
    dialog = _open_overlay(app) if app else None
    if dialog is None:
        return []
    names = []
    for button in buttons(dialog):
        name = (button.get_name() or "").strip()
        if name and name not in names:
            names.append(name)
    press_escape()
    wait_for_dialog_closed(pid)
    return names


def wait_for_dialog_closed(pid: int, timeout=3.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        app = rebind(pid)
        if app and not find_all(app, role="dialog"):
            return True
        time.sleep(0.1)
    return False


def _inside_dialog(node) -> bool:
    parent = node.get_parent()
    while parent is not None:
        try:
            if parent.get_role_name() == "dialog":
                return True
            parent = parent.get_parent()
        except Exception:
            return False
    return False


def sidebar_row(app, name):
    """A persistent-sidebar row for ``name`` with an AT-SPI action, or None."""
    for node in find_all(app):
        try:
            node_name = (node.get_name() or "").strip()
            if node.get_role_name() not in ACTIONABLE_ROLES:
                continue
            if node_name != name and not node_name.startswith(name):
                continue
            if _inside_dialog(node):
                continue
            action = node.get_action_iface()
            if action and action.get_n_actions() > 0:
                return node
        except Exception:
            continue
    return None


def navigate_to(pid: int, name) -> bool:
    app = rebind(pid)
    if app is None:
        return False
    row = sidebar_row(app, name)
    if row is not None:
        try:
            if row.get_action_iface().do_action(0):
                return True
        except Exception:
            pass
    dialog = _open_overlay(app)
    if dialog is None:
        return False
    if not click_button(dialog, name, timeout=3.0):
        press_escape()
        return False
    wait_for_dialog_closed(pid)
    return True


def destinations_visible(pid: int) -> bool:
    """True once Core exposes a sidebar row or lists destinations in the overlay.

    The launcher button alone is not evidence: Core shows it on the
    onboarding surfaces too, where its overlay lists no destination.
    """
    app = rebind(pid)
    if app is None:
        return False
    if any(sidebar_row(app, name) is not None for name in EXPECTED_DESTINATIONS):
        return True
    listed = set(overlay_destinations(pid))
    return len(listed & set(EXPECTED_DESTINATIONS)) >= 3
