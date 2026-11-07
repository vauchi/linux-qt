# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Canaries for screenshot comparison, independent of the GUI driver."""

import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

import screenshot


def write_image(path, changed=False):
    def chunk(kind, payload):
        checksum = zlib.crc32(kind + payload)
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)

    rows = bytearray()
    for y in range(20):
        rows.append(0)
        for x in range(20):
            value = 0 if changed and x < 10 and y < 10 else 255
            rows.extend((value, value, value))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 20, 20, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


class ScreenshotOracleTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        root = Path(self.directory.name)
        self.baseline = root / "baseline" / "screen.png"
        self.actual = root / "actual" / "screen.png"
        self.diff = root / "diff"
        self.enterContext(patch.multiple(screenshot, BASELINE_DIR=str(self.baseline.parent), DIFF_DIR=str(self.diff)))
        self.enterContext(patch.dict(os.environ, {"UPDATE_SNAPSHOTS": "0"}))
        write_image(self.actual)

    # @internal — a new capture must not certify itself as a visual reference (#1109).
    def test_missing_baseline_reports_no_comparison_without_creating_one(self):
        result = screenshot.check_against_baseline("screen.png", str(self.actual))
        self.assertEqual(result, "Missing baseline for 'screen.png'; visual comparison not performed.")
        self.assertFalse(self.baseline.exists())

    # @internal — equal images pass without altering their reference.
    def test_identical_images_match_without_changing_the_baseline(self):
        write_image(self.baseline)
        original = self.baseline.read_bytes()
        self.assertIsNone(screenshot.check_against_baseline("screen.png", str(self.actual)))
        self.assertEqual(self.baseline.read_bytes(), original)

    # @internal — seeded pixel changes must trip the real comparator (#1109).
    def test_changed_quarter_of_image_reports_exact_difference(self):
        write_image(self.baseline)
        original = self.baseline.read_bytes()
        write_image(self.actual, changed=True)
        result = screenshot.check_against_baseline("screen.png", str(self.actual))
        self.assertEqual(result.splitlines()[0], "'screen.png' changed: 25.0% pixel diff (threshold: 2.0%).")
        self.assertEqual(self.baseline.read_bytes(), original)
        self.assertTrue((self.diff / "screen.png").exists())

    # @internal — candidate generation is an explicit operator action.
    def test_explicit_generation_creates_a_candidate(self):
        with patch.dict(os.environ, {"UPDATE_SNAPSHOTS": "1"}):
            self.assertIsNone(screenshot.check_against_baseline("screen.png", str(self.actual)))
        self.assertEqual(self.baseline.read_bytes(), self.actual.read_bytes())

    # @internal — only explicit generation may replace an existing candidate.
    def test_explicit_update_replaces_the_candidate(self):
        write_image(self.baseline)
        write_image(self.actual, changed=True)
        with patch.dict(os.environ, {"UPDATE_SNAPSHOTS": "1"}):
            self.assertIsNone(screenshot.check_against_baseline("screen.png", str(self.actual)))
        self.assertEqual(self.baseline.read_bytes(), self.actual.read_bytes())


if __name__ == "__main__":
    unittest.main()
