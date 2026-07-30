# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Live accessibility contract for the Core-driven Qt command surface."""

import time

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402

from helpers import dump_tree, find_all, wait_for_element


def _buttons(root):
    nodes = find_all(root, role="push button")
    nodes.extend(find_all(root, role="button"))
    unique = {}
    for node in nodes:
        unique[id(node)] = node
    return list(unique.values())


def _press_key(keycode):
    Atspi.generate_keyboard_event(
        keycode, "", Atspi.KeySynthType.PRESSRELEASE
    )
    time.sleep(0.2)


def _press_ctrl_key(keycode):
    Atspi.generate_keyboard_event(37, "", Atspi.KeySynthType.PRESS)
    Atspi.generate_keyboard_event(
        keycode, "", Atspi.KeySynthType.PRESSRELEASE
    )
    Atspi.generate_keyboard_event(37, "", Atspi.KeySynthType.RELEASE)
    time.sleep(0.3)


class TestContextualSurface:
    """Verify the live native shell exposes only generic command controls."""

    def test_interactive_controls_are_named_and_actionable(self, qt_app):
        buttons = _buttons(qt_app)
        assert buttons, f"No native buttons found.\n{dump_tree(qt_app, 7)}"
        for button in buttons:
            assert (button.get_name() or "").strip(), (
                f"Unnamed native button.\n{dump_tree(qt_app, 7)}"
            )
            actions = button.get_action_iface()
            assert actions is not None
            assert actions.get_n_actions() > 0

    def test_frontend_navigation_list_is_absent(self, qt_app):
        navigation_lists = []
        for role in ("list", "list box", "tree"):
            navigation_lists.extend(
                node
                for node in find_all(qt_app, role=role)
                if (node.get_name() or "") == "Navigation"
            )
        assert not navigation_lists, (
            "Frontend-owned navigation list is still exposed.\n"
            f"{dump_tree(qt_app, 7)}"
        )

    def test_tab_reaches_a_native_command(self, qt_app):
        for _ in range(12):
            _press_key(23)
            focused = [
                button
                for button in _buttons(qt_app)
                if button.get_state_set().contains(Atspi.StateType.FOCUSED)
            ]
            if focused:
                assert (focused[0].get_name() or "").strip()
                return
        raise AssertionError(
            f"Tab did not focus a native command.\n{dump_tree(qt_app, 7)}"
        )

    def test_navigation_shortcut_opens_native_overlay(self, qt_app):
        _press_ctrl_key(45)
        dialog = wait_for_element(qt_app, role="dialog", timeout=3.0)
        assert dialog is not None, (
            "Ctrl+K did not open the Core-provided navigation overlay.\n"
            f"{dump_tree(qt_app, 8)}"
        )
        assert (dialog.get_name() or "").strip()
        overlay_buttons = _buttons(dialog)
        assert overlay_buttons
        assert all((button.get_name() or "").strip()
                   for button in overlay_buttons)
        _press_key(9)
