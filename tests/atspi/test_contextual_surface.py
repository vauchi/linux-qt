# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Live accessibility contract for the Core-driven Qt command surface."""

import re
import time

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402

from helpers import click_button, dump_tree, find_all, wait_for_element
from navigation import EXPECTED_DESTINATIONS, NAVIGATION_LAUNCHER, sidebar_row


def _buttons(root):
    nodes = find_all(root, role="push button")
    nodes.extend(find_all(root, role="button"))
    unique = {}
    for node in nodes:
        unique[id(node)] = node
    return list(unique.values())


def _focus_app(root):
    for button in _buttons(root):
        component = button.get_component_iface()
        if component is not None and component.grab_focus():
            time.sleep(0.2)
            return
    raise AssertionError(f"Could not focus the Qt app.\n{dump_tree(root, 7)}")


def _press_key(keycode):
    Atspi.generate_keyboard_event(
        keycode, "", Atspi.KeySynthType.PRESSRELEASE
    )
    time.sleep(0.2)


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
        _focus_app(qt_app)
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

    def test_destinations_are_offered_once_and_their_icons_stay_silent(self, qt_app):
        """Core's destinations reach AT-SPI once: as sidebar rows, or in the
        overlay the "More" launcher opens while the sidebar is hidden
        (vauchi/private#479). Either way no destination's icon is its own
        named object.

        Each destination shows a themed icon beside its word. The icon
        repeats what the word already says, so a screen reader should stop
        once, on the button. If it arrives on the bus as its own named
        object the reader stops twice, and the second stop announces a
        freedesktop icon name — "system users symbolic". The iOS shell had
        exactly this defect with SF Symbols (ios!649); Qt attaches the icon
        as a QAction property rather than a child widget and so should be
        immune, which is precisely the claim worth pinning.
        """
        _focus_app(qt_app)
        launchers = [
            button for button in _buttons(qt_app)
            if (button.get_name() or "").strip() == NAVIGATION_LAUNCHER
        ]
        sidebar = [sidebar_row(qt_app, name) for name in EXPECTED_DESTINATIONS]
        sidebar = [row for row in sidebar if row is not None]
        if sidebar:
            assert not launchers, (
                "The sidebar shows the destinations, yet a launcher is on screen.\n"
                f"{dump_tree(qt_app, 7)}"
            )
            destinations = sidebar
            scope = qt_app
        else:
            assert click_button(qt_app, NAVIGATION_LAUNCHER)
            dialog = wait_for_element(qt_app, role="dialog", timeout=3.0)
            assert dialog is not None, (
                "Navigation action did not open the Core-provided overlay.\n"
                f"{dump_tree(qt_app, 8)}"
            )
            assert (dialog.get_name() or "").strip()
            destinations = _buttons(dialog)
            scope = dialog
        assert destinations
        assert all((button.get_name() or "").strip() for button in destinations)

        # This assertion is expected green from the start and so never gets a
        # red run to prove the pattern can fire (CC-27); pin both directions
        # against a real theme icon name and a real destination label.
        icon_name_shape = re.compile(r"^[a-z0-9]+(-[a-z0-9]+)+$")
        assert icon_name_shape.match("system-users-symbolic")
        assert not icon_name_shape.match("My Card")
        offenders = [
            node.get_name()
            for destination in destinations
            for node in [destination, *find_all(destination)]
            if node.get_name() and icon_name_shape.match(node.get_name())
        ]
        assert not offenders, (
            f"These reach AT-SPI as icon names rather than words: {offenders}\n"
            f"{dump_tree(scope, 6)}"
        )
        _press_key(9)
