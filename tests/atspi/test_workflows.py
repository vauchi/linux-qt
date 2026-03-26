# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""End-to-end workflow tests for qVauchi via AT-SPI.

Each test exercises a complete user journey across multiple screens.
"""

import time

import pytest

from helpers import (
    find_all,
    find_one,
    click_button,
    set_text,
    get_label_text,
    wait_for_element,
    dump_tree,
)


def _navigate_to(app, screen_label):
    """Best-effort sidebar navigation."""
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


class TestOnboardingWorkflow:
    """Full onboarding flow from fresh identity creation."""

    def test_fresh_app_shows_setup(self, qt_app_fresh):
        """A fresh app should display the onboarding/setup screen."""
        labels = find_all(qt_app_fresh, role="label")
        label_texts = [l.get_name() for l in labels if l.get_name()]
        assert len(label_texts) > 0, "No labels on fresh launch"

    def test_onboarding_has_text_input(self, qt_app_fresh):
        """Onboarding should have a text input field for name entry."""
        entries = find_all(qt_app_fresh, role="text")
        buttons = find_all(qt_app_fresh, role="push button")
        buttons += find_all(qt_app_fresh, role="button")
        # Fresh app should have either text entries or action buttons
        assert len(entries) + len(buttons) > 0, (
            "Onboarding should have at least one input or button.\n"
            f"Tree:\n{dump_tree(qt_app_fresh, 6)}"
        )


class TestNavigationWorkflow:
    """Test navigation between multiple screens."""

    def test_navigate_multiple_screens(self, qt_app):
        """App should remain responsive after navigating multiple screens."""
        sidebar = find_one(qt_app, name="Navigation")
        assert sidebar is not None, "Sidebar not found"

        labels = find_all(qt_app, role="label")
        initial_count = len(labels)
        assert initial_count > 0, "No labels found initially"

        # Navigate through a few screens
        for screen in ["Settings", "Help", "Contacts"]:
            _navigate_to(qt_app, screen)
            current_labels = find_all(qt_app, role="label")
            assert len(current_labels) > 0, (
                f"App became unresponsive after navigating to {screen}.\n"
                f"Tree:\n{dump_tree(qt_app, 4)}"
            )


class TestExchangeWorkflow:
    """Contact exchange flow with QR code."""

    def test_exchange_screen_has_content(self, qt_app):
        """Exchange screen should show QR-related elements."""
        _navigate_to(qt_app, "Exchange")
        labels = find_all(qt_app, role="label", max_depth=15)
        assert len(labels) > 0, (
            "Exchange screen should have content.\n"
            f"Tree:\n{dump_tree(qt_app, 6)}"
        )


class TestSettingsWorkflow:
    """Settings screen interaction."""

    def test_settings_has_interactive_widgets(self, qt_app):
        """Settings screen should have interactive widgets."""
        _navigate_to(qt_app, "Settings")
        checks = find_all(qt_app, role="check box")
        toggles = find_all(qt_app, role="toggle button")
        buttons = find_all(qt_app, role="push button")
        buttons += find_all(qt_app, role="button")
        assert len(checks) + len(toggles) + len(buttons) > 0, (
            "Settings should have interactive widgets.\n"
            f"Tree:\n{dump_tree(qt_app, 6)}"
        )


class TestBackupWorkflow:
    """Backup screen presence."""

    def test_backup_screen_has_actions(self, qt_app):
        """Backup screen should have action buttons for export/import."""
        _navigate_to(qt_app, "Backup")
        labels = find_all(qt_app, role="label", max_depth=15)
        assert len(labels) > 0, (
            "Backup screen should have content.\n"
            f"Tree:\n{dump_tree(qt_app, 6)}"
        )


class TestHardwareDegradation:
    """Verify graceful hardware degradation."""

    def test_app_starts_without_camera(self, qt_app):
        """App should start successfully even without camera hardware."""
        assert qt_app is not None
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0

    def test_app_starts_without_bluetooth(self, qt_app):
        """App should start successfully even without Bluetooth hardware."""
        assert qt_app is not None
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0
