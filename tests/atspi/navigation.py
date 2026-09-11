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

from helpers import click_button, find_all, wait_for_element

# Core labels the context bar's navigation launcher "More" in the test
# locale (see test_contextual_surface.py).
NAVIGATION_LAUNCHER = "More"
EXPECTED_DESTINATIONS = ["Contacts", "My Card", "Exchange", "Devices", "Settings"]
ESCAPE_KEYCODE = 9
ACTIONABLE_ROLES = {"push button", "button", "check box", "toggle button", "radio button"}


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


def overlay_destinations(app) -> list[str]:
    """Destination labels listed by Core's navigation overlay."""
    dialog = _open_overlay(app)
    if dialog is None:
        return []
    names = []
    for button in buttons(dialog):
        name = (button.get_name() or "").strip()
        if name and name not in names:
            names.append(name)
    press_escape()
    wait_for_dialog_closed(app)
    return names


def wait_for_dialog_closed(app, timeout=3.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not find_all(app, role="dialog"):
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


def navigate_to(app, name) -> bool:
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
    wait_for_dialog_closed(app)
    return True


def destinations_visible(app) -> bool:
    """True once Core exposes the navigation launcher or a sidebar row."""
    if any((b.get_name() or "").strip() == NAVIGATION_LAUNCHER for b in buttons(app)):
        return True
    return any(sidebar_row(app, name) is not None for name in EXPECTED_DESTINATIONS)
