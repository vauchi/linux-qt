#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

# Generate C++ coverage report for linux-qt using gcov/lcov.
#
# Usage: ./scripts/coverage.sh
# Output: build/coverage/index.html

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-coverage"

echo "── Linux-Qt Coverage Report ──"

# Check dependencies
for cmd in cmake lcov genhtml; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "ERROR: $cmd not found. Install it first."
        echo "  lcov + genhtml: sudo pacman -S lcov  (or apt install lcov)"
        exit 1
    fi
done

# Clean build with coverage flags
echo "  Configuring with coverage..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCOVERAGE=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    2>&1 | tail -5

echo "  Building..."
cmake --build "$BUILD_DIR" -j"$(nproc)" 2>&1 | tail -3

# Reset counters
echo "  Resetting coverage counters..."
lcov --directory "$BUILD_DIR" --zerocounters --quiet 2>/dev/null || true

# Run tests
echo "  Running tests..."
(cd "$BUILD_DIR" && ctest --output-on-failure)

# Capture coverage data
echo "  Capturing coverage..."
lcov --directory "$BUILD_DIR" \
     --capture \
     --output-file "$BUILD_DIR/coverage.info" \
     --quiet

# Filter out system headers and test files
echo "  Filtering coverage data..."
lcov --remove "$BUILD_DIR/coverage.info" \
     '/usr/*' \
     '*/Qt*' \
     '*/tests/*' \
     '*/build-coverage/*' \
     --output-file "$BUILD_DIR/coverage-filtered.info" \
     --quiet

# Generate HTML report
echo "  Generating HTML report..."
mkdir -p "$BUILD_DIR/coverage"
genhtml "$BUILD_DIR/coverage-filtered.info" \
    --output-directory "$BUILD_DIR/coverage" \
    --title "qVauchi Coverage" \
    --quiet

# Summary
echo ""
echo "Coverage report: $BUILD_DIR/coverage/index.html"
lcov --summary "$BUILD_DIR/coverage-filtered.info" 2>&1 | grep -E "lines|functions"
