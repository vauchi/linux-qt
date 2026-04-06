# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Screen navigation and content verification tests for qVauchi via AT-SPI."""

from helpers import dump_tree, find_all, find_one


# Expected sidebar count — 5 top-level items.
# Labels come from i18n and may be "Missing: nav.*" in CI without bundled locale.
EXPECTED_SIDEBAR_COUNT = 5


class TestSidebarNavigation:
    """Test that sidebar items are present and well-formed."""

    def test_sidebar_has_expected_item_count(self, qt_app):
        """Sidebar should contain the expected number of entries."""
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, "Sidebar not found"

        items = find_all(sidebar, role="list item", max_depth=5)
        item_names = [i.get_name() for i in items if i.get_name()]
        assert len(item_names) >= EXPECTED_SIDEBAR_COUNT, (
            f"Expected >= {EXPECTED_SIDEBAR_COUNT} sidebar items, "
            f"found {len(item_names)}: {item_names}.\n"
            f"Tree:\n{dump_tree(sidebar, 4)}"
        )

    def test_sidebar_items_have_labels(self, qt_app):
        """Each sidebar item should have a non-empty accessible label."""
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, "Sidebar not found"

        items = find_all(sidebar, role="list item", max_depth=5)
        for item in items:
            name = item.get_name()
            assert name and len(name) > 0, (
                f"Sidebar item has empty accessible label.\n"
                f"Tree:\n{dump_tree(sidebar, 4)}"
            )

    def test_sidebar_items_have_action_interface(self, qt_app):
        """Each sidebar item should expose an AT-SPI action interface."""
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, "Sidebar not found"

        items = find_all(sidebar, role="list item", max_depth=5)
        assert len(items) > 0, "No sidebar items found"

        for item in items:
            action = item.get_action_iface()
            assert action is not None, (
                f"Sidebar item '{item.get_name()}' has no action interface.\n"
                f"Tree:\n{dump_tree(sidebar, 4)}"
            )


class TestScreenContent:
    """Verify key screens have expected component types."""

    def test_app_has_interactive_widgets(self, qt_app):
        """App should expose interactive widgets (buttons, toggles)."""
        checks = find_all(qt_app, role="check box")
        toggles = find_all(qt_app, role="toggle button")
        buttons = find_all(qt_app, role="push button")
        buttons += find_all(qt_app, role="button")
        assert len(checks) + len(toggles) + len(buttons) > 0, (
            f"App has no interactive widgets.\n"
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
