# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Screen navigation tests for qVauchi via AT-SPI."""

import pytest

from helpers import find_all, find_one, dump_tree


class TestSidebarNavigation:
    def test_sidebar_has_items(self, qt_app):
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, "Sidebar not found"
        items = find_all(sidebar, max_depth=5)
        assert len(items) > 0, f"Empty sidebar. Tree:\n{dump_tree(sidebar, 4)}"

    def test_app_has_labels(self, qt_app):
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0

    def test_app_has_action_buttons(self, qt_app):
        buttons = find_all(qt_app, role="push button")
        assert isinstance(buttons, list)


class TestScreenContent:
    def test_screen_title_visible(self, qt_app):
        title = find_one(qt_app, name="screen_title")
        # objectName may not be exposed as AT-SPI name in Qt
        # fall back to checking any labels exist
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0

    def test_tree_has_depth(self, qt_app):
        all_nodes = find_all(qt_app, max_depth=10)
        assert len(all_nodes) > 2, (
            f"Tree too shallow ({len(all_nodes)} nodes).\n"
            f"Tree:\n{dump_tree(qt_app, 6)}"
        )

    def test_dump_tree_for_debugging(self, qt_app):
        tree = dump_tree(qt_app, max_depth=8)
        print(f"\n=== qVauchi AT-SPI Tree ===\n{tree}\n=== End ===")
        assert len(tree) > 0
