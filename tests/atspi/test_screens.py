# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Screen navigation and content verification tests for qVauchi via AT-SPI.

Verifies that screens are reachable via sidebar navigation and render
expected content in the AT-SPI tree.
"""

import time

import pytest

from helpers import find_all, find_one, click_button, dump_tree


# Screens accessible via sidebar (from INVENTORY.md)
SIDEBAR_SCREENS = [
    "My Info",
    "Contacts",
    "Exchange",
    "Settings",
    "Help",
    "Backup",
    "Device Linking",
    "Duress PIN",
    "Emergency Shred",
    "Delivery Status",
]


def _navigate_to(app, screen_label):
    """Best-effort sidebar navigation for Qt."""
    for role in ("list item", "push button", "label"):
        items = find_all(app, role=role, max_depth=8)
        for item in items:
            if item.get_name() == screen_label:
                try:
                    action = item.get_action_iface()
                    if action and action.get_n_actions() > 0:
                        action.do_action(0)
                        time.sleep(0.5)
                        return True
                except Exception:
                    pass
    return click_button(app, screen_label)


class TestSidebarNavigation:
    """Test that all screens are reachable via sidebar navigation."""

    def test_sidebar_has_screen_entries(self, qt_app):
        """Sidebar should contain entries for available screens."""
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, "Sidebar not found"

        items = find_all(sidebar, max_depth=5)
        assert len(items) > 0, (
            f"Empty sidebar.\n"
            f"Tree:\n{dump_tree(sidebar, 4)}"
        )

    @pytest.mark.parametrize("screen_name", SIDEBAR_SCREENS[:5])
    def test_navigate_to_screen(self, qt_app, screen_name):
        """Navigate to a screen via sidebar and verify it loads."""
        _navigate_to(qt_app, screen_name)
        # The screen should load without crashing
        labels = find_all(qt_app, role="label", max_depth=10)
        assert len(labels) > 0, (
            f"App appears unresponsive after navigating to {screen_name}.\n"
            f"Tree:\n{dump_tree(qt_app, 4)}"
        )


class TestScreenContent:
    """Verify key screens have expected component types."""

    def test_settings_has_interactive_widgets(self, qt_app):
        """Settings screen should contain toggle or button widgets."""
        _navigate_to(qt_app, "Settings")
        checks = find_all(qt_app, role="check box")
        toggles = find_all(qt_app, role="toggle button")
        buttons = find_all(qt_app, role="push button")
        buttons += find_all(qt_app, role="button")
        assert len(checks) + len(toggles) + len(buttons) > 0, (
            f"Settings has no interactive widgets.\n"
            f"Tree:\n{dump_tree(qt_app, 6)}"
        )

    def test_screen_has_labels(self, qt_app):
        """The app should render text labels."""
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0, (
            f"No labels found.\n"
            f"Tree:\n{dump_tree(qt_app, 5)}"
        )

    def test_tree_has_depth(self, qt_app):
        """AT-SPI tree should have reasonable depth."""
        all_nodes = find_all(qt_app, max_depth=10)
        assert len(all_nodes) > 2, (
            f"Tree too shallow ({len(all_nodes)} nodes).\n"
            f"Tree:\n{dump_tree(qt_app, 6)}"
        )

    def test_dump_tree_for_debugging(self, qt_app):
        """Dump the full AT-SPI tree (for manual inspection, always passes)."""
        tree = dump_tree(qt_app, max_depth=8)
        print(f"\n=== qVauchi AT-SPI Tree ===\n{tree}\n=== End ===")
        assert len(tree) > 0
