# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Visual snapshot capture of the qvauchi onboarding flow.

Launches an unseeded app (no ``--reset-for-testing``), captures each
onboarding step into ``snapshots/actual/onboarding-<n>-<title>.png``,
drives the step's primary action (typing a display name where an input
is present) and stops once Core exposes the navigation destinations.

Lives in its own module so the module-scoped seeded ``qt_app`` of
``test_snapshots.py`` is never on the Xvfb display at the same time —
captures are of the root window, so two windows would overlap.
"""

import time

import pytest

from helpers import click_button, dump_tree, find_all, find_app, set_text
from navigation import buttons, destinations_visible
from screenshot import (
    ACTUAL_DIR,
    capture_stable,
    check_against_baseline,
    file_sha256,
    screenshot_tool,
    slug,
)

# Ordered by preference: the flow's forward action first, entry points last.
PRIMARY_ACTIONS = [
    "Continue",
    "Start using the app",
    "Create new identity",
    "Get started",
    "Next",
    "Done",
]
DISPLAY_NAME = "Snapshot Explorer"
MAX_STEPS = 8
MIN_DISTINCT_CAPTURES = 3


def _screen_title(app) -> str:
    for label in find_all(app, role="label"):
        name = (label.get_name() or "").strip()
        if name and not name.startswith("Missing:"):
            return name
    return "untitled"


def _type_display_name(app) -> bool:
    for entry in find_all(app, role="text"):
        name = (entry.get_name() or "").strip()
        if name and set_text(app, name, DISPLAY_NAME, timeout=2.0):
            return True
    return False


def _primary_action(app) -> str | None:
    names = {(b.get_name() or "").strip() for b in buttons(app)}
    for candidate in PRIMARY_ACTIONS:
        if candidate in names:
            return candidate
    return None


def _refresh(app):
    # Qt re-registers its AT-SPI tree across surface swaps, so re-bind the
    # root by pid the way test_reliability.py does after every click.
    return find_app("vauchi", timeout=5.0, pid=app.get_process_id()) or app


def test_snapshot_onboarding_flow(qt_app_fresh):
    assert screenshot_tool(), "No screenshot tool available (grim or ImageMagick import)"

    app = qt_app_fresh
    captured: list[str] = []
    hashes: list[str] = []
    regressions: list[str] = []
    for step in range(1, MAX_STEPS + 1):
        app = _refresh(app)
        if destinations_visible(app):
            break
        title = _screen_title(app)
        filename = f"onboarding-{step}-{slug(title)}.png"

        # Type before capturing so the step's screenshot shows a filled form.
        typed = _type_display_name(app)
        if typed:
            time.sleep(0.2)  # let the entry repaint before the capture

        actual_path = capture_stable(filename, ACTUAL_DIR)
        assert actual_path, f"Screenshot capture failed at step {step} ({title})"
        captured.append(filename)
        hashes.append(file_sha256(actual_path))
        failure = check_against_baseline(filename, actual_path)
        if failure:
            regressions.append(f"Onboarding {failure}")

        action = _primary_action(app)
        if action is None or not click_button(app, action, timeout=3.0):
            break
        time.sleep(0.3)  # AT-SPI reports the new surface slightly after the click

    assert captured, (
        "No onboarding screenshots captured: destinations were visible on "
        "first paint or no primary action was found.\n" + dump_tree(app, 8)
    )
    assert destinations_visible(app), (
        f"Onboarding did not reach the navigation destinations after "
        f"{len(captured)} step(s) {captured}.\n" + dump_tree(app, 8)
    )
    assert len(set(hashes)) >= MIN_DISTINCT_CAPTURES, (
        f"Onboarding produced too few distinct screens: {len(set(hashes))} "
        f"unique capture(s) across {captured}"
    )
    if regressions:
        pytest.fail("\n".join(regressions))
