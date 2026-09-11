# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Screenshot capture and comparison utilities for the qvauchi AT-SPI tests.

Mirrors ``linux-gtk/tests/atspi/screenshot.py``: captures the Xvfb root
window with ``grim`` (Wayland) or ImageMagick ``import`` (X11), and
compares two captures with ImageMagick ``compare``.
"""

import hashlib
import os
import shutil
import subprocess
import time

SNAPSHOTS_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "snapshots")
BASELINE_DIR = os.path.join(SNAPSHOTS_ROOT, "baseline")
ACTUAL_DIR = os.path.join(SNAPSHOTS_ROOT, "actual")
DIFF_DIR = os.path.join(SNAPSHOTS_ROOT, "diff")

# Pixel difference threshold (0.0 = exact match, 1.0 = completely different).
# Qt rendering has minor anti-aliasing variance across runs — allow small diff.
DIFF_THRESHOLD = 0.02


def screenshot_tool() -> str | None:
    """Name of the first screenshot tool on PATH, or None."""
    for tool in ("grim", "import"):
        if shutil.which(tool):
            return tool
    return None


def take_screenshot(filename: str, output_dir: str = "screenshots") -> str | None:
    """Capture a screenshot of the current display.

    Tries grim (Wayland) first, then import (X11/ImageMagick).
    Returns the full path to the saved screenshot, or None on failure.
    """
    os.makedirs(output_dir, exist_ok=True)
    filepath = os.path.join(output_dir, filename)

    if shutil.which("grim"):
        result = subprocess.run(["grim", filepath], capture_output=True, timeout=10)
        if result.returncode == 0:
            return filepath

    if shutil.which("import"):
        result = subprocess.run(
            ["import", "-window", "root", filepath],
            capture_output=True,
            timeout=10,
        )
        if result.returncode == 0:
            return filepath

    return None


def capture_stable(filename, output_dir=ACTUAL_DIR, attempts=6, interval=0.15):
    """Capture until two consecutive frames are byte-identical.

    AT-SPI reports the new screen before Qt necessarily finishes painting
    it, so a single capture can catch a half-rendered frame. Poll until the
    rendered frame stops changing; return the last capture if it never
    settles so the comparison surfaces the real instability.
    """
    prev_bytes = None
    path = None
    for _ in range(attempts):
        path = take_screenshot(filename, output_dir=output_dir)
        if path is None:
            return None
        with open(path, "rb") as fh:
            cur = fh.read()
        if prev_bytes is not None and cur == prev_bytes:
            return path
        prev_bytes = cur
        time.sleep(interval)  # poll interval for frame stability, not a fixed wait
    return path


def file_sha256(path: str) -> str:
    with open(path, "rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


def parse_ae(stderr: str):
    """Parse the pixel count from ImageMagick ``compare -metric AE`` output.

    ImageMagick 7 prints ``"1234 (0.0188)"``; older builds print a bare
    integer. Take the leading token so both forms parse. Returns ``None``
    when there is no parsable leading number (caller treats as full diff).
    """
    try:
        return float(stderr.strip().split()[0])
    except (ValueError, AttributeError, IndexError):
        return None


def compare_images(baseline_path: str, actual_path: str, diff_path: str) -> float:
    """Return the pixel difference ratio between two images (0.0 .. 1.0)."""
    os.makedirs(os.path.dirname(diff_path), exist_ok=True)

    result = subprocess.run(
        [
            "compare",
            "-fuzz", "2%",    # anti-aliasing tolerance
            "-metric", "AE",  # absolute error (pixel count)
            baseline_path,
            actual_path,
            diff_path,
        ],
        capture_output=True,
        text=True,
        timeout=30,
    )
    diff_pixels = parse_ae(result.stderr)
    if diff_pixels is None:
        return 1.0

    dims = subprocess.run(
        ["identify", "-format", "%w %h", baseline_path],
        capture_output=True,
        text=True,
        timeout=10,
    )
    try:
        w, h = dims.stdout.strip().split()
        total_pixels = int(w) * int(h)
        return diff_pixels / total_pixels if total_pixels > 0 else 1.0
    except (ValueError, AttributeError):
        return 1.0


def check_against_baseline(filename: str, actual_path: str) -> str | None:
    """Compare a capture with its baseline; create the baseline when absent.

    Baselines are optional: a missing one is recorded from the capture
    (as is every one when ``UPDATE_SNAPSHOTS=1``) and never fails. Returns
    a failure message when an existing baseline diverges beyond
    ``DIFF_THRESHOLD``, else ``None``.
    """
    baseline_path = os.path.join(BASELINE_DIR, filename)
    updating = os.environ.get("UPDATE_SNAPSHOTS", "") == "1"
    if updating or not os.path.exists(baseline_path):
        os.makedirs(BASELINE_DIR, exist_ok=True)
        shutil.copy2(actual_path, baseline_path)
        return None

    diff_path = os.path.join(DIFF_DIR, filename)
    diff_ratio = compare_images(baseline_path, actual_path, diff_path)
    if diff_ratio <= DIFF_THRESHOLD:
        return None
    return (
        f"'{filename}' changed: {diff_ratio:.1%} pixel diff "
        f"(threshold: {DIFF_THRESHOLD:.1%}).\n"
        f"  Baseline: {baseline_path}\n"
        f"  Actual:   {actual_path}\n"
        f"  Diff:     {diff_path}\n"
        "To update: UPDATE_SNAPSHOTS=1 ./run-tests.sh -k test_snapshots"
    )


def slug(name: str) -> str:
    """File-name-safe form of a screen title."""
    cleaned = "".join(ch if ch.isalnum() else "-" for ch in name.strip().lower())
    return "-".join(part for part in cleaned.split("-") if part) or "untitled"
