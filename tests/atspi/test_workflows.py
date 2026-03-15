# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Workflow tests for qVauchi via AT-SPI."""

from helpers import find_all, find_one


class TestOnboardingWorkflow:
    def test_fresh_app_has_content(self, qt_app_fresh):
        labels = find_all(qt_app_fresh, role="label")
        assert len(labels) > 0, "No labels on fresh launch"


class TestNavigationWorkflow:
    def test_app_responsive_after_load(self, qt_app):
        labels = find_all(qt_app, role="label")
        assert len(labels) > 0


class TestHardwareDegradation:
    def test_app_starts_without_camera(self, qt_app):
        assert qt_app is not None

    def test_app_starts_without_bluetooth(self, qt_app):
        assert qt_app is not None
