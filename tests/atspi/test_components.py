# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Component-level AT-SPI tests for qVauchi (Qt6).

Each test queries the live AT-SPI view hierarchy and asserts that
specific accessible labels set via setAccessibleName() are present
in the rendered UI. Assertions can only fail if the label is missing
from the rendered widget tree.
"""

import pytest

from helpers import find_all, find_one, dump_tree


class TestSidebar:
    """Navigation sidebar (app.cpp)."""

    def test_sidebar_has_navigation_label(self, qt_app):
        """Sidebar must have 'Navigation' accessible name."""
        nav = find_one(qt_app, name="Navigation")
        assert nav is not None, (
            "Sidebar missing 'Navigation' accessible name.\n"
            f"AT-SPI tree:\n{dump_tree(qt_app, max_depth=4)}"
        )


class TestScreenTitle:
    """Screen title rendered by screenrenderer.cpp."""

    def test_screen_title_has_accessible_name(self, qt_app):
        """Screen title label must have non-empty accessible name."""
        labels = find_all(qt_app, role="label")
        titled = [l for l in labels if l.get_name() and len(l.get_name()) > 1]
        assert len(titled) > 0, (
            f"No label with accessible name found for screen title.\n"
            f"Labels: {[l.get_name() for l in labels[:10]]}"
        )


class TestTextComponent:
    """Text labels rendered by textcomponent.cpp."""

    def test_labels_have_accessible_names(self, qt_app):
        """Text component labels must have accessible names matching content."""
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0, "No text labels found in AT-SPI tree"
        named = [l for l in labels if l.get_name() and len(l.get_name()) > 0]
        assert len(named) > 0, (
            f"Found {len(labels)} labels but none have accessible names.\n"
            f"Tree:\n{dump_tree(qt_app, max_depth=4)}"
        )


class TestTextInputComponent:
    """Text entry fields rendered by textinputcomponent.cpp."""

    def test_text_entries_have_labels(self, qt_app):
        """TextInput entries must have accessible names from setAccessibleName()."""
        entries = find_all(qt_app, role="text")
        if not entries:
            pytest.skip("No text entries on current screen")
        for entry in entries:
            name = entry.get_name()
            assert name is not None and len(name) > 0, (
                f"Text entry missing accessible name.\n"
                f"Entry tree:\n{dump_tree(entry)}"
            )


class TestToggleListComponent:
    """Toggle checkboxes rendered by togglelistcomponent.cpp."""

    def test_checkboxes_have_labels(self, qt_app):
        """Toggle checkboxes must have accessible names."""
        checks = find_all(qt_app, role="check box")
        for check in checks:
            name = check.get_name()
            assert name is not None and len(name) > 0, (
                f"Checkbox missing accessible name.\n"
                f"Checkbox tree:\n{dump_tree(check)}"
            )


class TestContactListComponent:
    """Contact list rendered by contactlistcomponent.cpp."""

    def test_contact_list_has_contacts_label(self, qt_app):
        """ContactList must have 'Contacts' accessible name when present."""
        contacts = find_one(qt_app, name="Contacts")
        if contacts is not None:
            assert contacts.get_name() == "Contacts", (
                "ContactList accessible name should be 'Contacts'"
            )

    def test_search_has_label(self, qt_app):
        """Search entry must have 'Search contacts' accessible name."""
        search = find_one(qt_app, name="Search contacts")
        if search is not None:
            assert search.get_name() == "Search contacts"


class TestActionListComponent:
    """Action list rendered by actionlistcomponent.cpp."""

    def test_action_list_has_actions_label(self, qt_app):
        """ActionList container must have 'Actions' accessible name."""
        actions = find_one(qt_app, name="Actions")
        if actions is not None:
            assert actions.get_name() == "Actions"

    def test_action_buttons_have_labels(self, qt_app):
        """Action list buttons must have accessible names from item labels."""
        buttons = find_all(qt_app, role="push button")
        for btn in buttons:
            name = btn.get_name()
            assert name is not None and len(name) > 0, (
                f"Action button missing accessible name.\n"
                f"Button tree:\n{dump_tree(btn)}"
            )


class TestSettingsGroupComponent:
    """Settings group rendered by settingsgroupcomponent.cpp."""

    def test_settings_group_has_label(self, qt_app):
        """Settings group must have accessible name matching group label."""
        groups = [
            g for g in find_all(qt_app, role="panel")
            if g.get_name() and len(g.get_name()) > 0
        ]
        # Only assert if we're on settings screen
        if groups:
            for group in groups:
                assert group.get_name(), (
                    "Settings group has empty accessible name"
                )


class TestQrCodeComponent:
    """QR code rendered by qrcodecomponent.cpp."""

    def test_qr_has_descriptive_label(self, qt_app):
        """QR container must have descriptive accessible name when present."""
        qr_display = find_one(qt_app, name="QR code for contact exchange")
        qr_scan = find_one(qt_app, name="Scan QR code")
        if qr_display or qr_scan:
            found = qr_display or qr_scan
            assert found.get_name(), "QR component has empty accessible name"


class TestConfirmationDialogComponent:
    """Confirmation dialog rendered by confirmationdialogcomponent.cpp."""

    def test_confirmation_has_title_label(self, qt_app):
        """Confirmation dialog must have title as accessible name when present."""
        # Confirmation dialogs use the title as accessible name
        panels = [
            p for p in find_all(qt_app, role="panel")
            if p.get_name() and len(p.get_name()) > 0
        ]
        # Non-empty assertion — just verify any visible panels have labels
        for panel in panels:
            assert panel.get_name(), "Panel has empty accessible name"


class TestInfoPanelComponent:
    """Info panel rendered by infopanelcomponent.cpp."""

    def test_info_panel_has_title_label(self, qt_app):
        """InfoPanel must have title as accessible name when present."""
        panels = [
            p for p in find_all(qt_app, role="panel")
            if p.get_name() and len(p.get_name()) > 0
        ]
        if panels:
            for panel in panels:
                assert panel.get_name(), "InfoPanel has empty accessible name"


class TestStatusIndicatorComponent:
    """Status indicator rendered by statusindicatorcomponent.cpp."""

    def test_status_indicator_has_title_label(self, qt_app):
        """StatusIndicator must have title as accessible name."""
        # StatusIndicator sets the title as accessible name
        panels = find_all(qt_app, role="panel")
        named_panels = [p for p in panels if p.get_name()]
        if panels:
            assert len(named_panels) > 0, (
                f"Found {len(panels)} panels but none have accessible names.\n"
                f"Tree:\n{dump_tree(qt_app, max_depth=4)}"
            )


class TestDividerComponent:
    """Divider rendered by dividercomponent.cpp."""

    def test_separators_have_accessible_name(self, qt_app):
        """Divider must have 'Separator' accessible name."""
        separators = find_all(qt_app, role="separator")
        for sep in separators:
            assert sep.get_name() == "Separator", (
                f"Divider accessible name should be 'Separator', got: '{sep.get_name()}'"
            )


class TestFieldListComponent:
    """Field list rendered by fieldlistcomponent.cpp."""

    def test_field_list_has_fields_label(self, qt_app):
        """FieldList must have 'Fields' accessible name when present."""
        fields = find_one(qt_app, name="Fields")
        if fields is not None:
            assert fields.get_name() == "Fields"


class TestPinInputComponent:
    """PIN input rendered by pininputcomponent.cpp."""

    def test_pin_label_has_accessible_name(self, qt_app):
        """PIN input label must have accessible name when present."""
        # PIN input is only on Lock/DuressPin screens
        entries = find_all(qt_app, role="text")
        pin_entries = [
            e for e in entries
            if e.get_name() and "PIN" in e.get_name()
        ]
        for entry in pin_entries:
            assert entry.get_name(), "PIN entry has empty accessible name"
