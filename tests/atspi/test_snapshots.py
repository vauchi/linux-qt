# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Visual snapshot capture for every qvauchi navigation destination.

Captures a screenshot of each Core-provided destination under Xvfb into
``snapshots/actual/`` and compares against ``snapshots/baseline/`` only
when a baseline exists (baselines are optional and are recorded on first
run). The captures are uploaded as CI artifacts by ``test:snapshots``.

Destinations are discovered from Core's contextual-navigation overlay
and reached through the persistent sidebar rows when they expose an
AT-SPI action, otherwise through the launcher + overlay buttons.

Usage:
  UPDATE_SNAPSHOTS=1 ./run-tests.sh -k test_snapshots -v   # record baselines
  ./run-tests.sh -k test_snapshots -v                       # verify
"""

import os

import pytest

from helpers import dump_tree, wait_for_element
from navigation import (
    describe_desktop,
    navigate_to,
    overlay_destinations,
    rebind,
    sidebar_destinations,
)
from screenshot import (
    ACTUAL_DIR,
    capture_stable,
    check_against_baseline,
    file_sha256,
    parse_ae,
    screenshot_tool,
    slug,
)

MIN_DISTINCT_CAPTURES = 3

# Screens whose pixels derive from the per-run test identity (avatars,
# colours hashed from key material) — captured, but never baselined.
NONDETERMINISTIC_SCREENS = {"Contacts", "My Card"}


class TestScreenSnapshots:
    """Capture every navigation destination the harness can reach."""

    def test_snapshot_all_navigation_destinations(self, qt_app):
        tool = screenshot_tool()
        assert tool, (
            "No screenshot tool available. Install grim (Wayland) or "
            "imagemagick (X11/Xvfb, provides `import`). PATH seen by pytest: "
            + os.environ.get("PATH", "<unset>")
        )

        pid = qt_app.get_process_id()
        print("Initial tree:\n" + dump_tree(qt_app, 8))
        screen_names = sidebar_destinations(qt_app) or overlay_destinations(pid)
        print(f"Destinations: {screen_names}\nDesktop:\n{describe_desktop()}")
        assert screen_names, (
            "Neither sidebar nor overlay listed a destination.\n"
            + dump_tree(rebind(pid) or qt_app, 8)
        )
        if any(name.startswith("Missing:") for name in screen_names):
            pytest.skip(
                f"Navigation labels are i18n fallbacks: {screen_names} — "
                "locale bundle failed to load; test infra issue, not a regression."
            )

        nav_failed: list[str] = []
        shot_failed: list[str] = []
        regressions: list[str] = []
        captured_hashes: dict[str, str] = {}
        for screen in screen_names:
            if not navigate_to(pid, screen):
                nav_failed.append(screen)
                print(f"Navigation to {screen!r} failed. Desktop:\n{describe_desktop()}")
                continue
            # The surface title is a label named after the destination;
            # absence is not fatal (Core may title the screen differently).
            app = rebind(pid)
            if app is not None:
                wait_for_element(app, role="label", name=screen, timeout=2.0)

            filename = f"{slug(screen)}.png"
            actual_path = capture_stable(filename, ACTUAL_DIR)
            if actual_path is None:
                shot_failed.append(screen)
                continue
            captured_hashes[screen] = file_sha256(actual_path)

            if screen in NONDETERMINISTIC_SCREENS:
                continue
            failure = check_against_baseline(filename, actual_path)
            if failure:
                regressions.append(f"Screen {failure}")

        if not captured_hashes:
            pytest.fail(
                "No screenshots captured for any of "
                f"{len(screen_names)} navigation destinations.\n"
                f"  Navigation failed: {nav_failed or 'none'}\n"
                f"  Screenshot capture failed: {shot_failed or 'none'}\n"
                f"  Screenshot tool: {tool}\n"
                + dump_tree(rebind(pid) or qt_app, 8)
            )

        distinct = set(captured_hashes.values())
        assert len(distinct) >= MIN_DISTINCT_CAPTURES, (
            "Navigation produced too few distinct screens: "
            f"{len(distinct)} unique capture(s) across {len(captured_hashes)} "
            f"navigated screen(s) {sorted(captured_hashes)}; "
            f"navigation failed for {nav_failed or 'none'}. The AT-SPI action "
            "may be a no-op (every capture shows the same initial screen)."
        )
        assert not regressions, "\n".join(regressions)


# Regression: ImageMagick 7 prints `compare -metric AE` as "<count> (<norm>)".
@pytest.mark.parametrize(
    "stderr,expected",
    [
        ("0 (0)", 0.0),
        ("1234 (0.0188)", 1234.0),
        ("0", 0.0),
        ("4096\n", 4096.0),
    ],
)
def test_parse_ae_accepts_imagemagick_formats(stderr, expected):
    assert parse_ae(stderr) == expected


@pytest.mark.parametrize("stderr", ["", "   ", "compare: images too dissimilar"])
def test_parse_ae_returns_none_for_unparsable(stderr):
    assert parse_ae(stderr) is None


@pytest.mark.parametrize(
    "title,expected",
    [("My Card", "my-card"), ("What would you like to do?", "what-would-you-like-to-do"), ("", "untitled")],
)
def test_slug_is_filename_safe(title, expected):
    assert slug(title) == expected
