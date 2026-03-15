# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Pytest fixtures for AT-SPI GUI testing of qvauchi."""

import os
import subprocess
import tempfile

import pytest

from helpers import find_app


@pytest.fixture(scope="session")
def qt_binary():
    """Path to the compiled qvauchi binary."""
    workspace = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    # Qt builds go to build/ directory
    for path in [
        os.path.join(workspace, "build", "qvauchi"),
        os.path.join(workspace, "build", "release", "qvauchi"),
        os.path.join(workspace, "build", "debug", "qvauchi"),
    ]:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path

    pytest.skip("qvauchi binary not found — run cmake build first")


@pytest.fixture
def data_dir():
    """Create a temporary data directory for isolated test runs."""
    with tempfile.TemporaryDirectory(prefix="vauchi-qt-test-") as tmpdir:
        yield tmpdir


@pytest.fixture
def qt_app(qt_binary, data_dir):
    """Launch qvauchi and return the AT-SPI accessible root.

    The app is launched with:
    - QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 (enable AT-SPI on Wayland)
    - QT_ACCESSIBILITY=1 (enable accessibility)
    - XDG_DATA_HOME=<tmpdir> (isolated storage)
    """
    env = os.environ.copy()
    env["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    env["QT_ACCESSIBILITY"] = "1"
    env["XDG_DATA_HOME"] = data_dir

    if "DISPLAY" not in env and "WAYLAND_DISPLAY" not in env:
        pytest.skip("No display available — run under Xvfb or with a desktop session")

    proc = subprocess.Popen(
        [qt_binary],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    # Wait for app to appear in AT-SPI tree
    app_root = find_app("qvauchi", timeout=15.0)
    if app_root is None:
        proc.kill()
        stdout, stderr = proc.communicate(timeout=5)
        pytest.fail(
            f"qvauchi did not appear in AT-SPI tree within 15s.\n"
            f"stdout: {stdout.decode()[:500]}\n"
            f"stderr: {stderr.decode()[:500]}"
        )

    yield app_root

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


@pytest.fixture
def qt_app_fresh(qt_binary):
    """Launch qvauchi with a fresh (empty) data directory."""
    data_dir = tempfile.mkdtemp(prefix="vauchi-qt-test-fresh-")
    env = os.environ.copy()
    env["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    env["QT_ACCESSIBILITY"] = "1"
    env["XDG_DATA_HOME"] = data_dir

    if "DISPLAY" not in env and "WAYLAND_DISPLAY" not in env:
        pytest.skip("No display available")

    proc = subprocess.Popen(
        [qt_binary],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    app_root = find_app("qvauchi", timeout=15.0)
    if app_root is None:
        proc.kill()
        stdout, stderr = proc.communicate(timeout=5)
        pytest.fail(
            f"qvauchi did not appear in AT-SPI tree.\n"
            f"stderr: {stderr.decode()[:500]}"
        )

    yield app_root

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)

    import shutil
    shutil.rmtree(data_dir, ignore_errors=True)
