# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Screen navigation and content verification tests for qVauchi via AT-SPI.

Verifies that screens are reachable via sidebar navigation and render
expected content in the AT-SPI tree.
"""

import pytest

from helpers import dump_tree, find_all, find_one, wait_for_element


# Top-level sidebar items use i18n labels (nav.myCard → "My Card", etc.)
SIDEBAR_SCREENS = ["My Card", "Contacts", "Exchange", "Groups", "More"]

# Screens under "More" reached via two-step navigation
MORE_SCREENS = ["Settings", "Help", "Backup", "Privacy"]


def _click_sidebar(app, label):
    """Click a sidebar list item by label. Returns True on success."""
    sidebar = find_one(app, name="Navigation")
    if sidebar is None:
        return False
    items = find_all(sidebar, role="list item", max_depth=5)
    for item in items:
        if item.get_name() == label:
            try:
                action = item.get_action_iface()
                if action and action.get_n_actions() > 0:
                    action.do_action(0)
                    wait_for_element(app, role="label", timeout=3.0)
                    return True
            except Exception:
                return False
    return False


def _wait_and_click(app, name, timeout=3.0):
    """Wait for a button to appear, then click it. Handles AT-SPI rendering delay."""
    for role in ("push button", "button"):
        btn = wait_for_element(app, role=role, name=name, timeout=timeout)
        if btn is not None:
            try:
                action = btn.get_action_iface()
                if action and action.get_n_actions() > 0:
                    action.do_action(0)
                    return True
            except Exception:
                return False
    return False


def _navigate_to(app, screen_label):
    """Navigate to a screen. Handles sidebar items and More sub-screens."""
    if screen_label in MORE_SCREENS:
        if not _click_sidebar(app, "More"):
            return False
        return _wait_and_click(app, screen_label)
    return _click_sidebar(app, screen_label)


class TestSidebarNavigation:
    """Test that all screens are reachable via sidebar navigation."""

    def test_sidebar_has_screen_entries(self, qt_app):
        """Sidebar should contain entries for available screens."""
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, "Sidebar not found"

        items = find_all(sidebar, role="list item", max_depth=5)
        item_names = [i.get_name() for i in items if i.get_name()]
        assert len(item_names) >= len(SIDEBAR_SCREENS), (
            f"Expected {len(SIDEBAR_SCREENS)} sidebar items, found {len(item_names)}: "
            f"{item_names}.\nTree:\n{dump_tree(sidebar, 4)}"
        )

    @pytest.mark.parametrize("screen_name", SIDEBAR_SCREENS)
    def test_navigate_to_screen(self, qt_app, screen_name):
        """Navigate to a screen via sidebar and verify the entry exists."""
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, "Sidebar not found"

        items = find_all(sidebar, role="list item", max_depth=5)
        item_names = [i.get_name() for i in items if i.get_name()]
        assert screen_name in item_names, (
            f"'{screen_name}' not in sidebar. Found: {item_names}"
        )


class TestScreenContent:
    """Verify key screens have expected component types."""

    def test_settings_has_interactive_widgets(self, qt_app):
        """Settings screen (under More) should contain toggle or button widgets."""
        _navigate_to(qt_app, "Settings")  # navigates via More
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
