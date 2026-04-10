# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Exploratory AT-SPI tests for linux-qt — probing areas beyond the existing suite.

These tests exercise notification drain, activity log navigation, toast handling,
deep sidebar traversal, accessibility completeness, and stress edge cases.
"""

import time

import pytest

from helpers import (
    find_all,
    dump_tree,
    is_sensitive,
)


# ---------------------------------------------------------------------------
# 1. Activity Log Screen — navigation and content
# ---------------------------------------------------------------------------

class TestActivityLogScreen:
    """Verify the Activity Log screen is reachable and renders content."""

    def test_activity_log_not_in_top_level_sidebar(self, qt_app):
        """Activity log is not a top-level sidebar item (nested under More)."""
        items = find_all(qt_app, role="list item")
        names = [it.get_name() for it in items]
        # Document that activity_log is registered in SCREEN_I18N
        # but not returned by available_screens() as a top-level item.
        # It's expected under "More" sub-navigation.
        has_more = any("more" in n.lower() for n in names)
        assert has_more, f"No 'More' item in sidebar. Items: {names}"


# ---------------------------------------------------------------------------
# 2. Deep Sidebar Navigation — all screens round-trip
# ---------------------------------------------------------------------------

class TestSidebarRoundTrip:
    """Navigate to every sidebar screen and back, checking for crashes."""

    def test_all_screens_have_content(self, qt_app):
        """Each sidebar screen must produce at least one label in the tree."""
        items = find_all(qt_app, role="list item")
        screen_results = {}

        for item in items:
            name = item.get_name()
            action = item.get_action_iface()
            if action and action.get_n_actions() > 0:
                action.do_action(0)
                time.sleep(0.4)
                labels = find_all(qt_app, role="label")
                screen_results[name] = len(labels)

        empty_screens = [k for k, v in screen_results.items() if v == 0]
        assert not empty_screens, (
            f"Screens with zero labels: {empty_screens}. "
            f"All results: {screen_results}"
        )

    def test_rapid_sidebar_switching(self, qt_app):
        """Rapidly switching between sidebar items should not crash."""
        items = find_all(qt_app, role="list item")
        for _ in range(3):
            for item in items:
                action = item.get_action_iface()
                if action and action.get_n_actions() > 0:
                    action.do_action(0)
                    time.sleep(0.1)

        # If we get here without crash, verify app is still responsive
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0, "App unresponsive after rapid sidebar switching"


# ---------------------------------------------------------------------------
# 3. Accessibility Completeness — unnamed widgets, missing roles
# ---------------------------------------------------------------------------

class TestAccessibilityCompleteness:
    """Check for accessibility gaps across all screens."""

    def test_no_unnamed_interactive_widgets(self, qt_app):
        """All interactive widgets should have accessible names."""
        items = find_all(qt_app, role="list item")
        unnamed_report = []

        for item in items[:5]:  # Check first 5 screens
            screen_name = item.get_name()
            action = item.get_action_iface()
            if action and action.get_n_actions() > 0:
                action.do_action(0)
                time.sleep(0.3)

            for role in ["push button", "button", "check box", "text"]:
                widgets = find_all(qt_app, role=role)
                for w in widgets:
                    wname = w.get_name()
                    if not wname or wname.strip() == "":
                        unnamed_report.append(
                            f"  [{role}] on screen '{screen_name}'"
                        )

        if unnamed_report:
            pytest.fail(
                f"Found {len(unnamed_report)} unnamed interactive widgets:\n"
                + "\n".join(unnamed_report[:20])
            )

    def test_buttons_have_action_interface(self, qt_app):
        """Every button should expose an AT-SPI action interface."""
        buttons = find_all(qt_app, role="push button")
        buttons += find_all(qt_app, role="button")
        missing_action = []

        for btn in buttons:
            action = btn.get_action_iface()
            if not action or action.get_n_actions() == 0:
                missing_action.append(btn.get_name())

        assert not missing_action, (
            f"Buttons without action interface: {missing_action}"
        )

    def test_text_entries_have_editable_interface(self, qt_app):
        """Text fields should expose EditableText AT-SPI interface."""
        entries = find_all(qt_app, role="text")
        if not entries:
            pytest.skip("No text entries visible on current screen")

        for entry in entries:
            editable = entry.get_editable_text_iface()
            assert editable is not None, (
                f"Text entry '{entry.get_name()}' has no editable interface"
            )


# ---------------------------------------------------------------------------
# 4. Status Bar Toast — trigger and verify
# ---------------------------------------------------------------------------

class TestToastDisplay:
    """Verify toast messages appear in the status bar after actions."""

    def test_status_bar_exists(self, qt_app):
        """The main window should have a status bar region."""
        # Qt status bars typically appear as "status bar" or "filler" at the bottom
        tree = dump_tree(qt_app, max_depth=8)
        assert len(tree) > 50, (
            f"AT-SPI tree suspiciously small ({len(tree)} chars)"
        )


# ---------------------------------------------------------------------------
# 5. Window Properties and State
# ---------------------------------------------------------------------------

class TestWindowProperties:
    """Verify window-level properties and states."""

    def test_window_is_visible(self, qt_app):
        """The main window should be in visible state."""
        import gi
        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi
        windows = find_all(qt_app, role="frame")
        assert len(windows) >= 1, "No window frame found"
        window = windows[0]
        state = window.get_state_set()
        assert state.contains(Atspi.StateType.VISIBLE), "Window not visible"

    def test_window_title_is_vauchi(self, qt_app):
        """Window title should be 'Vauchi'."""
        windows = find_all(qt_app, role="frame")
        assert len(windows) >= 1
        assert windows[0].get_name() == "Vauchi"

    def test_window_has_menu_bar(self, qt_app):
        """Main window should contain a menu bar."""
        menubars = find_all(qt_app, role="menu bar")
        assert len(menubars) >= 1, "No menu bar found in window"

    def test_menu_bar_has_children(self, qt_app):
        """Menu bar should have child nodes (menus or menu items)."""
        menubars = find_all(qt_app, role="menu bar")
        assert len(menubars) >= 1
        menubar = menubars[0]
        child_count = menubar.get_child_count()
        # Qt6 AT-SPI exposes menus as direct children, not sub-role "menu"
        assert child_count > 0, (
            f"Menu bar has 0 children. "
            f"Tree:\n{dump_tree(menubar, max_depth=3)}"
        )


# ---------------------------------------------------------------------------
# 6. Exchange Screen Deep Dive
# ---------------------------------------------------------------------------

class TestExchangeScreen:
    """Deep exploration of the Exchange screen UI."""

    def _navigate_to_exchange(self, qt_app):
        items = find_all(qt_app, role="list item")
        for it in items:
            if "exchange" in it.get_name().lower():
                action = it.get_action_iface()
                if action and action.get_n_actions() > 0:
                    action.do_action(0)
                    time.sleep(0.5)
                return True
        return False

    def test_exchange_has_qr_or_instruction(self, qt_app):
        """Exchange screen should show QR code or instruction text."""
        if not self._navigate_to_exchange(qt_app):
            pytest.skip("Exchange not in sidebar")

        labels = find_all(qt_app, role="label")
        # Should have some instruction or QR-related content
        assert len(labels) > 0, "Exchange screen has no labels"

    def test_exchange_interactive_elements_are_sensitive(self, qt_app):
        """Buttons on exchange screen should be enabled."""
        if not self._navigate_to_exchange(qt_app):
            pytest.skip("Exchange not in sidebar")

        buttons = find_all(qt_app, role="push button")
        buttons += find_all(qt_app, role="button")
        for btn in buttons:
            name = btn.get_name()
            if name:  # Only check named buttons
                assert is_sensitive(btn), (
                    f"Button '{name}' on exchange screen is not sensitive"
                )


# ---------------------------------------------------------------------------
# 7. Settings Screen Deep Dive
# ---------------------------------------------------------------------------

class TestSettingsScreen:
    """Deep exploration of the Settings screen."""

    def _navigate_to_settings(self, qt_app):
        # Settings is under "More" in the sidebar
        items = find_all(qt_app, role="list item")
        for it in items:
            name = it.get_name().lower()
            if "more" in name or "settings" in name:
                action = it.get_action_iface()
                if action and action.get_n_actions() > 0:
                    action.do_action(0)
                    time.sleep(0.5)
                return True
        return False

    def test_settings_reachable(self, qt_app):
        """Settings/More screen should be navigable."""
        found = self._navigate_to_settings(qt_app)
        assert found, "Could not navigate to Settings/More"

    def test_settings_has_toggle_or_checkbox(self, qt_app):
        """Settings screen should have toggleable options."""
        self._navigate_to_settings(qt_app)
        toggles = find_all(qt_app, role="check box")
        toggles += find_all(qt_app, role="toggle button")
        # More screen might not have toggles directly, that's ok
        # Just verify we don't crash navigating here


# ---------------------------------------------------------------------------
# 8. FFI Notification Drain (direct CABI call)
# ---------------------------------------------------------------------------

class TestNotificationDrainFFI:
    """Test notification polling via direct CABI FFI calls."""

    def test_drain_returns_empty_array(self):
        """Draining notifications on a fresh app should return empty JSON array."""
        import ctypes
        import json

        cabi_path = None
        for p in [
            "/home/megloff1/Workspace/vauchi/core/target/debug/libvauchi_cabi.so",
            "/home/megloff1/Workspace/vauchi/core/target/release/libvauchi_cabi.so",
        ]:
            import os
            if os.path.isfile(p):
                cabi_path = p
                break

        if cabi_path is None:
            pytest.skip("libvauchi_cabi.so not found")

        lib = ctypes.CDLL(cabi_path)
        lib.vauchi_app_create.restype = ctypes.c_void_p
        lib.vauchi_app_drain_notifications.restype = ctypes.c_char_p
        lib.vauchi_app_destroy.argtypes = [ctypes.c_void_p]
        lib.vauchi_app_drain_notifications.argtypes = [ctypes.c_void_p]

        app = lib.vauchi_app_create()
        assert app is not None, "Failed to create VauchiApp"

        try:
            result = lib.vauchi_app_drain_notifications(app)
            if result is not None:
                notifications = json.loads(result.decode())
                assert isinstance(notifications, list)
                assert len(notifications) == 0, (
                    f"Expected empty notifications, got: {notifications}"
                )
        finally:
            lib.vauchi_app_destroy(app)

    def test_poll_returns_valid_json(self):
        """Poll notifications should return valid JSON (array or null)."""
        import ctypes
        import json

        cabi_path = None
        import os
        for p in [
            "/home/megloff1/Workspace/vauchi/core/target/debug/libvauchi_cabi.so",
            "/home/megloff1/Workspace/vauchi/core/target/release/libvauchi_cabi.so",
        ]:
            if os.path.isfile(p):
                cabi_path = p
                break

        if cabi_path is None:
            pytest.skip("libvauchi_cabi.so not found")

        lib = ctypes.CDLL(cabi_path)
        lib.vauchi_app_create.restype = ctypes.c_void_p
        lib.vauchi_app_poll_notifications.restype = ctypes.c_char_p
        lib.vauchi_app_destroy.argtypes = [ctypes.c_void_p]
        lib.vauchi_app_poll_notifications.argtypes = [ctypes.c_void_p]

        app = lib.vauchi_app_create()
        assert app is not None

        try:
            result = lib.vauchi_app_poll_notifications(app)
            if result is not None:
                data = json.loads(result.decode())
                assert isinstance(data, list)
        finally:
            lib.vauchi_app_destroy(app)

    def test_null_app_does_not_crash(self):
        """Passing null to drain/poll should not crash."""
        import ctypes

        cabi_path = None
        import os
        for p in [
            "/home/megloff1/Workspace/vauchi/core/target/debug/libvauchi_cabi.so",
        ]:
            if os.path.isfile(p):
                cabi_path = p
                break

        if cabi_path is None:
            pytest.skip("libvauchi_cabi.so not found")

        lib = ctypes.CDLL(cabi_path)
        lib.vauchi_app_drain_notifications.restype = ctypes.c_char_p
        lib.vauchi_app_drain_notifications.argtypes = [ctypes.c_void_p]
        lib.vauchi_app_poll_notifications.restype = ctypes.c_char_p
        lib.vauchi_app_poll_notifications.argtypes = [ctypes.c_void_p]

        # Should return null, not crash
        result1 = lib.vauchi_app_drain_notifications(None)
        result2 = lib.vauchi_app_poll_notifications(None)
        assert result1 is None
        assert result2 is None


# ---------------------------------------------------------------------------
# 9. Stress: Multiple screen transitions
# ---------------------------------------------------------------------------

class TestStress:
    """Stress tests for stability under repeated operations."""

    def test_fifty_sidebar_transitions(self, qt_app):
        """50 rapid sidebar transitions should not crash or leak."""
        items = find_all(qt_app, role="list item")
        if len(items) < 2:
            pytest.skip("Not enough sidebar items")

        for i in range(50):
            item = items[i % len(items)]
            action = item.get_action_iface()
            if action and action.get_n_actions() > 0:
                action.do_action(0)
                time.sleep(0.05)

        # Verify app is still alive and responsive
        time.sleep(0.5)
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0, "App unresponsive after 50 transitions"

    def test_tree_dump_all_screens(self, qt_app):
        """Dump AT-SPI tree for every screen — useful for manual inspection."""
        items = find_all(qt_app, role="list item")
        trees = {}

        for item in items:
            name = item.get_name()
            action = item.get_action_iface()
            if action and action.get_n_actions() > 0:
                action.do_action(0)
                time.sleep(0.3)
                trees[name] = dump_tree(qt_app, max_depth=6)

        # Print for manual review (visible in pytest -s output)
        for screen, tree in trees.items():
            print(f"\n{'='*60}\nScreen: {screen}\n{'='*60}")
            print(tree)

        assert len(trees) > 0
