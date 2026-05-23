# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Visual snapshot tests for qVauchi screens.

Captures screenshots of each screen under Xvfb and compares against
committed baselines. On first run (no baselines), generates them.
On subsequent runs, diffs against baselines and fails if pixels
diverge beyond threshold.

Usage:
  # Generate baselines (first run or after UI changes):
  UPDATE_SNAPSHOTS=1 ./run-tests.sh -k test_snapshots -v

  # Verify against baselines (CI):
  ./run-tests.sh -k test_snapshots -v
"""

import os
import shutil
import subprocess
import tempfile

import pytest

from helpers import dump_tree, find_all, find_app, find_one, wait_for_element
from screenshot import take_screenshot

# Per-theme baselines live in baseline/<theme>/, where <theme> is one of
# `dark` or `light`. The legacy flat baseline/ layout is gone — the
# dark-mode snapshot parity record (2026-05-01) standardises on this
# layout so the CI gate can compare each theme independently.
SNAPSHOTS_ROOT = os.path.join(os.path.dirname(__file__), "snapshots")

# Pixel difference threshold (0.0 = exact match, 1.0 = completely different).
# Qt rendering may have minor variance across runs — allow small diff.
DIFF_THRESHOLD = 0.02  # 2% pixel difference allowed

# Sidebar items use i18n labels (nav.myCard → "My Card", etc.).
# More sub-screens excluded — AT-SPI do_action(0) on QListWidget items doesn't
# trigger currentRowChanged, so sidebar clicks don't navigate to More.
SNAPSHOT_SCREENS = ["My Card", "Contacts", "Exchange", "Groups"]

# Themes the dark-mode-parity gate exercises. Each value is the literal
# string passed via VAUCHI_THEME to qvauchi at launch; main.cpp /
# app.cpp routes anything other than "light" to the dark default.
SNAPSHOT_THEMES = ["dark", "light"]


def _baseline_dir(theme: str) -> str:
    return os.path.join(SNAPSHOTS_ROOT, "baseline", theme)


def _actual_dir(theme: str) -> str:
    return os.path.join(SNAPSHOTS_ROOT, "actual", theme)


def _diff_dir(theme: str) -> str:
    return os.path.join(SNAPSHOTS_ROOT, "diff", theme)


def _screen_filename(name: str) -> str:
    return f"{name.lower().replace(' ', '_')}.png"


def _navigate_to(app, screen_label):
    """Navigate to a sidebar screen via AT-SPI action."""
    sidebar = find_one(app, name="Navigation")
    if sidebar is None:
        return False
    items = find_all(sidebar, role="list item", max_depth=5)
    for item in items:
        if item.get_name() == screen_label:
            try:
                action = item.get_action_iface()
                if action and action.get_n_actions() > 0:
                    action.do_action(0)
                    wait_for_element(app, role="label", timeout=3.0)
                    return True
            except Exception:
                return False
    return False


def _compare_images(baseline_path: str, actual_path: str, diff_path: str) -> float:
    """Compare two images and return pixel difference ratio.

    Uses ImageMagick `compare` for perceptual diff. Returns 0.0 for
    identical images, up to 1.0 for completely different.
    """
    import subprocess

    os.makedirs(os.path.dirname(diff_path), exist_ok=True)

    result = subprocess.run(
        [
            "compare",
            "-fuzz", "2%",    # Allow 2% color diff (anti-aliasing tolerance)
            "-metric", "AE",  # Absolute Error (pixel count)
            baseline_path,
            actual_path,
            diff_path,
        ],
        capture_output=True,
        text=True,
        timeout=30,
    )

    # `compare` writes pixel count to stderr
    try:
        diff_pixels = int(result.stderr.strip())
    except (ValueError, AttributeError):
        return 1.0  # Can't parse — treat as full diff

    # Get image dimensions to compute ratio
    result2 = subprocess.run(
        ["identify", "-format", "%w %h", baseline_path],
        capture_output=True,
        text=True,
        timeout=10,
    )
    try:
        w, h = result2.stdout.strip().split()
        total_pixels = int(w) * int(h)
        return diff_pixels / total_pixels if total_pixels > 0 else 1.0
    except (ValueError, AttributeError):
        return 1.0


@pytest.fixture
def qt_app_themed(qt_binary, request):
    """Launch a per-test qvauchi instance with a chosen theme.

    The session-scoped `qt_app` fixture in conftest.py can't help here —
    the theme is bootstrapped once in app.cpp at startup, so switching
    themes requires a fresh process. This fixture pays the per-test
    startup cost (~3-5s on CI) only for the snapshot tests that need it.

    Parameterized via `request.param`, which the test class passes as
    one of `SNAPSHOT_THEMES`.
    """
    theme = request.param
    data_dir = tempfile.mkdtemp(prefix=f"vauchi-qt-snapshot-{theme}-")
    env = os.environ.copy()
    env["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    env["QT_ACCESSIBILITY"] = "1"
    env["XDG_DATA_HOME"] = data_dir
    env["VAUCHI_THEME"] = theme

    if "DISPLAY" not in env and "WAYLAND_DISPLAY" not in env:
        pytest.skip("No display available — run under Xvfb or with a desktop session")

    proc = subprocess.Popen(
        [qt_binary, "--reset-for-testing"],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    app_root = find_app("vauchi", timeout=15.0)
    if app_root is None:
        proc.kill()
        stdout, stderr = proc.communicate(timeout=5)
        pytest.fail(
            f"qvauchi (VAUCHI_THEME={theme}) did not appear in AT-SPI tree "
            f"within 15s.\nstderr: {stderr.decode()[:500]}"
        )

    yield app_root, theme

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)

    shutil.rmtree(data_dir, ignore_errors=True)


class TestScreenSnapshots:
    """Capture and compare screenshots for each screen × theme pair.

    Theme parametrization is the dark-mode-parity gate (record
    `2026-05-01-dark-mode-snapshot-parity`): every sidebar screen
    must have a baseline under both `baseline/dark/` and
    `baseline/light/`. A regression that only inverts under one
    color scheme (e.g. a hardcoded `#1e1e2e` background that ignores
    `bg-primary`) fails one half of the pair and is caught here.
    """

    @pytest.mark.parametrize(
        "qt_app_themed", SNAPSHOT_THEMES, indirect=True
    )
    @pytest.mark.parametrize("screen", SNAPSHOT_SCREENS)
    def test_screen_snapshot(self, qt_app_themed, screen):
        """Screenshot each screen and compare against the per-theme baseline."""
        app, theme = qt_app_themed
        baseline_dir = _baseline_dir(theme)
        actual_dir = _actual_dir(theme)
        diff_dir = _diff_dir(theme)

        navigated = _navigate_to(app, screen)
        assert navigated, (
            f"Failed to navigate to '{screen}' (theme={theme}). "
            f"Sidebar may be missing this entry — is an identity present?\n"
            f"Tree:\n{dump_tree(app, 4)}"
        )

        filename = _screen_filename(screen)
        os.makedirs(actual_dir, exist_ok=True)
        actual_path = take_screenshot(filename, output_dir=actual_dir)

        if actual_path is None:
            pytest.skip("Screenshot capture not available (no import/grim)")

        baseline_path = os.path.join(baseline_dir, filename)
        updating = os.environ.get("UPDATE_SNAPSHOTS", "") == "1"

        if updating or not os.path.exists(baseline_path):
            os.makedirs(baseline_dir, exist_ok=True)
            shutil.copy2(actual_path, baseline_path)
            if updating:
                pytest.skip(f"Baseline updated: {theme}/{filename}")
            else:
                pytest.skip(
                    f"Baseline created: {theme}/{filename} — commit and re-run"
                )

        diff_path = os.path.join(diff_dir, filename)
        diff_ratio = _compare_images(baseline_path, actual_path, diff_path)

        assert diff_ratio <= DIFF_THRESHOLD, (
            f"Screen '{screen}' (theme={theme}) changed: "
            f"{diff_ratio:.1%} pixel diff (threshold: {DIFF_THRESHOLD:.1%}).\n"
            f"  Baseline: {baseline_path}\n"
            f"  Actual:   {actual_path}\n"
            f"  Diff:     {diff_path}\n"
            f"To update: UPDATE_SNAPSHOTS=1 ./run-tests.sh -k test_snapshots"
        )
