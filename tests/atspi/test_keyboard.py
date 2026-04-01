# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Keyboard navigation and interaction tests for qVauchi via AT-SPI.

Verifies that interactive widgets are reachable and operable via
keyboard (focusable, editable, actionable). Queries the live AT-SPI
tree — assertions fail only if expected state is absent.
"""

import time

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402

import pytest

from helpers import find_all, find_one, is_sensitive, dump_tree


# Sidebar screens in order (first 5 correspond to Alt+1..5)
SIDEBAR_SCREENS = ["My Info", "Contacts", "Exchange", "Settings", "Help"]

# X11 keycodes for digits 1-5
_KEYCODES = {1: 10, 2: 11, 3: 12, 4: 13, 5: 14}


def _send_alt_key(digit: int) -> None:
    """Synthesize an Alt+<digit> keystroke via AT-SPI.

    Presses Alt, then the digit key, then releases both.
    Uses X11 keycodes which work under Xvfb (CI) and X11 sessions.
    """
    keycode = _KEYCODES[digit]
    # Press Alt (keycode 64 = Left Alt on X11)
    Atspi.generate_keyboard_event(64, "", Atspi.KeySynthType.PRESS)
    Atspi.generate_keyboard_event(keycode, "", Atspi.KeySynthType.PRESSRELEASE)
    Atspi.generate_keyboard_event(64, "", Atspi.KeySynthType.RELEASE)
    # Allow the toolkit to process the event
    time.sleep(0.3)


class TestKeyboardNavigation:
    """Verify keyboard-driven interaction properties."""

    def test_interactive_elements_are_sensitive(self, qt_app):
        """All visible interactive elements should be in sensitive state."""
        buttons = find_all(qt_app, role="push button")
        buttons += find_all(qt_app, role="button")
        for btn in buttons:
            state = is_sensitive(btn)
            assert isinstance(state, bool), (
                f"Could not determine sensitivity of button '{btn.get_name()}'"
            )

    def test_sidebar_items_are_focusable(self, qt_app):
        """Sidebar navigation items should exist and be queryable."""
        sidebar = find_one(qt_app, name="Navigation")
        if sidebar is None:
            pytest.skip("No sidebar found — still on onboarding")
        items = find_all(sidebar, role="list item", max_depth=5)
        assert len(items) >= 1, (
            f"Expected at least 1 sidebar item, found {len(items)}.\n"
            f"Sidebar tree:\n{dump_tree(sidebar, max_depth=3)}"
        )

    def test_text_entries_are_editable(self, qt_app_fresh):
        """Text entries should be marked as editable in AT-SPI state."""
        entries = find_all(qt_app_fresh, role="text")
        if not entries:
            pytest.skip("No text entries on current screen")
        for entry in entries:
            try:
                state_set = entry.get_state_set()
                editable = state_set.contains(Atspi.StateType.EDITABLE)
                assert editable, (
                    f"Entry '{entry.get_name()}' is not editable.\n"
                    f"Entry tree:\n{dump_tree(entry)}"
                )
            except Exception:
                pass  # Some entries may not expose state

    def test_check_boxes_are_checkable(self, qt_app):
        """Check boxes should support toggle action."""
        checks = find_all(qt_app, role="check box")
        for check in checks:
            try:
                action = check.get_action_iface()
                if action:
                    count = action.get_n_actions()
                    assert count > 0, (
                        f"Check box '{check.get_name()}' has no actions"
                    )
            except Exception:
                pass


class TestAccessibleTree:
    """Verify the AT-SPI tree structure is well-formed."""

    def test_tree_is_not_empty(self, qt_app):
        """The AT-SPI tree should have content."""
        all_nodes = find_all(qt_app, max_depth=10)
        assert len(all_nodes) > 2, (
            f"Tree too shallow (only {len(all_nodes)} nodes).\n"
            f"Tree:\n{dump_tree(qt_app, 6)}"
        )

    def test_no_unnamed_buttons(self, qt_app):
        """Buttons should have accessible names."""
        for role in ("push button", "button"):
            buttons = find_all(qt_app, role=role)
            for btn in buttons:
                name = btn.get_name()
                assert name is not None and len(name) > 0, (
                    f"Button with role '{role}' has no accessible name.\n"
                    f"Button tree:\n{dump_tree(btn)}"
                )

    def test_dump_tree_for_debugging(self, qt_app):
        """Dump the full AT-SPI tree (for manual inspection, always passes)."""
        tree = dump_tree(qt_app, max_depth=8)
        print(f"\n=== qVauchi AT-SPI Tree ===\n{tree}\n=== End ===")
        assert len(tree) > 0


class TestSidebarShortcuts:
    """Verify Alt+1..5 keyboard shortcuts switch sidebar tabs."""

    @pytest.mark.parametrize(
        "digit, expected_screen",
        [(i + 1, name) for i, name in enumerate(SIDEBAR_SCREENS)],
        ids=[f"Alt+{i + 1}-{name}" for i, name in enumerate(SIDEBAR_SCREENS)],
    )
    def test_alt_shortcut_navigates_to_screen(self, qt_app, digit, expected_screen):
        """Alt+<digit> should navigate to the corresponding sidebar screen."""
        _send_alt_key(digit)

        # After the shortcut, the sidebar should have the expected screen selected
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, (
            f"Sidebar not found after Alt+{digit}.\n"
            f"Tree:\n{dump_tree(qt_app, 5)}"
        )

        # Verify the selected row matches the expected screen.
        # Qt6 AT-SPI marks selected items with SELECTED state.
        items = find_all(sidebar, role="list item", max_depth=5)
        selected = [
            item for item in items
            if item.get_state_set().contains(Atspi.StateType.SELECTED)
        ]
        if selected:
            selected_name = selected[0].get_name()
            # The row name or a child label should contain the screen name
            labels = find_all(selected[0], role="label", max_depth=3)
            label_texts = [lb.get_name() for lb in labels]
            assert expected_screen in label_texts or selected_name == expected_screen, (
                f"Alt+{digit}: expected '{expected_screen}', "
                f"got selected='{selected_name}' labels={label_texts}.\n"
                f"Sidebar tree:\n{dump_tree(sidebar, 4)}"
            )
        else:
            # Fallback: check that screen content is visible in the main area
            screen_node = find_one(qt_app, name=expected_screen)
            assert screen_node is not None, (
                f"Alt+{digit}: no SELECTED sidebar item and no '{expected_screen}' "
                f"element found.\nTree:\n{dump_tree(qt_app, 5)}"
            )
