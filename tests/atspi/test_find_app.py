# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Regression: find_app must disambiguate co-resident apps by PID.

Guards 2026-06-05-linux-qt-snapshot-find-app-collision. Two qvauchi
processes (e.g. the session-scoped ``qt_app`` plus a per-test
``qt_app_themed`` instance) both register in the AT-SPI desktop under
the application name ``"vauchi"``. A name-only ``find_app("vauchi")``
returns whichever appears first, so the themed snapshot fixture bound
to — navigated, and screenshotted — the WRONG window (dark session
instance instead of its own light instance). The fix adds a ``pid``
filter.

Lives in its own module (not test_snapshots.py) on purpose: CI's
``test:a11y`` job runs ``-k "not test_snapshots"`` and is BLOCKING,
so this guard runs there. ``test:snapshots`` is a separate job.
"""

import os
import shutil
import subprocess
import tempfile

import pytest

from helpers import find_app


def _spawn(qt_binary: str, theme: str) -> tuple[subprocess.Popen, str]:
    """Launch a qvauchi with its own data dir and theme; return (proc, dir)."""
    data_dir = tempfile.mkdtemp(prefix=f"vauchi-findapp-{theme}-")
    env = os.environ.copy()
    env["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    env["QT_ACCESSIBILITY"] = "1"
    env["XDG_DATA_HOME"] = data_dir
    env["VAUCHI_THEME"] = theme
    proc = subprocess.Popen(
        [qt_binary, "--reset-for-testing"],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return proc, data_dir


def test_find_app_disambiguates_coresident_apps_by_pid(qt_binary):
    """Two ``"vauchi"`` apps coexist; ``find_app(pid=…)`` returns the match.

    Asserts each lookup returns the accessible whose process id equals
    the requested pid (not merely the first name match), that the two
    are distinct, and that a pid owning no app yields None rather than
    falling back to a name match.
    """
    if "DISPLAY" not in os.environ and "WAYLAND_DISPLAY" not in os.environ:
        pytest.skip("No display available — run under Xvfb or a desktop session")

    procs: list[tuple[subprocess.Popen, str]] = []
    try:
        procs.append(_spawn(qt_binary, "dark"))
        procs.append(_spawn(qt_binary, "light"))
        (p_dark, _), (p_light, _) = procs

        app_dark = find_app("vauchi", timeout=15.0, pid=p_dark.pid)
        app_light = find_app("vauchi", timeout=15.0, pid=p_light.pid)

        assert app_dark is not None, "find_app(pid=dark) found no matching app"
        assert app_light is not None, "find_app(pid=light) found no matching app"
        assert app_dark.get_process_id() == p_dark.pid
        assert app_light.get_process_id() == p_light.pid
        assert app_dark.get_process_id() != app_light.get_process_id()

        # A pid that owns no app must NOT silently fall back to a name match.
        orphan = find_app("vauchi", timeout=1.0, pid=2_000_000_000)
        assert orphan is None, "find_app(pid=<unowned>) should not name-match"
    finally:
        for proc, data_dir in procs:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=5)
            shutil.rmtree(data_dir, ignore_errors=True)
