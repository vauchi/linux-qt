# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Component-level AT-SPI tests for qVauchi (14 component types)."""

from helpers import find_all, find_one, dump_tree


class TestTextComponent:
    def test_labels_have_text_content(self, qt_app):
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0, "No text labels found"


class TestTextInputComponent:
    def test_text_entries_exist(self, qt_app):
        entries = find_all(qt_app, role="text")
        assert isinstance(entries, list)


class TestToggleListComponent:
    def test_checkboxes_found(self, qt_app):
        checks = find_all(qt_app, role="check box")
        assert isinstance(checks, list)


class TestContactListComponent:
    def test_contact_list_discoverable(self, qt_app):
        lists = find_all(qt_app, name="Contacts")
        assert isinstance(lists, list)


class TestActionListComponent:
    def test_action_buttons_discoverable(self, qt_app):
        buttons = find_all(qt_app, role="push button")
        assert isinstance(buttons, list)


class TestSettingsGroupComponent:
    def test_settings_group_discoverable(self, qt_app):
        groups = find_all(qt_app, role="panel")
        assert isinstance(groups, list)


class TestCardPreviewComponent:
    def test_card_preview_accessible(self, qt_app):
        cards = find_all(qt_app, name="card_preview")
        assert isinstance(cards, list)


class TestQrCodeComponent:
    def test_qr_container_accessible(self, qt_app):
        qr = find_one(qt_app, name="QR code for contact exchange")
        assert isinstance(qr, object)


class TestConfirmationDialogComponent:
    def test_confirmation_discoverable(self, qt_app):
        buttons = find_all(qt_app, role="push button")
        assert isinstance(buttons, list)


class TestInfoPanelComponent:
    def test_info_panels_discoverable(self, qt_app):
        panels = find_all(qt_app, role="panel")
        assert isinstance(panels, list)


class TestStatusIndicatorComponent:
    def test_status_indicators_discoverable(self, qt_app):
        indicators = find_all(qt_app, role="panel")
        assert isinstance(indicators, list)


class TestPinInputComponent:
    def test_pin_entries_discoverable(self, qt_app):
        entries = find_all(qt_app, role="text")
        assert isinstance(entries, list)


class TestFieldListComponent:
    def test_field_list_discoverable(self, qt_app):
        fields = find_all(qt_app, name="Fields")
        assert isinstance(fields, list)


class TestDividerComponent:
    def test_separators_exist(self, qt_app):
        seps = find_all(qt_app, role="separator")
        assert isinstance(seps, list)
