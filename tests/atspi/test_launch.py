# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Launch and basic AT-SPI tree verification tests for qVauchi."""

from helpers import dump_tree, find_all


class TestAppLaunch:
    """Verify the app launches and appears in the AT-SPI tree."""

    def test_app_appears_in_atspi_tree(self, qt_app):
        assert qt_app is not None
        assert qt_app.get_name() == "vauchi"

    def test_app_has_window(self, qt_app):
        windows = find_all(qt_app, role="frame", max_depth=2)
        assert len(windows) >= 1, f"No window found. Tree:\n{dump_tree(qt_app, 3)}"

    def test_window_has_title(self, qt_app):
        windows = find_all(qt_app, role="frame", max_depth=2)
        window_names = [w.get_name() or "" for w in windows]
        assert any("Vauchi" in name for name in window_names), (
            f"No window with 'Vauchi' in name. Found: {window_names}"
        )

    def test_screen_title_exists(self, qt_app):
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0, "No labels found in the app"
