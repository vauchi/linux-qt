# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Drives `qvauchi --render-fixture` on a golden ScreenModel fixture and asserts
# a valid, non-trivial PNG comes out. Guards the design screenshot catalog
# harness (problem 2026-06-12-device-screenshot-catalog). Invoked via add_test
# with -DBIN / -DFIXTURE / -DPNG_OUT / -DCABI_DIR.

if(NOT EXISTS "${FIXTURE}")
  message(FATAL_ERROR "golden fixture missing: ${FIXTURE} — is core/ checked out?")
endif()

file(REMOVE "${PNG_OUT}")
set(ENV{QT_QPA_PLATFORM} offscreen)
set(ENV{LD_LIBRARY_PATH} "${CABI_DIR}:$ENV{LD_LIBRARY_PATH}")

execute_process(
  COMMAND "${BIN}" --render-fixture "${FIXTURE}" "${PNG_OUT}" 900 1400
  RESULT_VARIABLE rc
  ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "render-fixture exited ${rc}: ${err}")
endif()

if(NOT EXISTS "${PNG_OUT}")
  message(FATAL_ERROR "render-fixture produced no output at ${PNG_OUT}")
endif()

file(SIZE "${PNG_OUT}" png_size)
if(png_size LESS 2000)
  message(FATAL_ERROR "PNG suspiciously small (${png_size} bytes) — likely a blank frame")
endif()

file(READ "${PNG_OUT}" magic LIMIT 4 HEX)
if(NOT magic STREQUAL "89504e47")
  message(FATAL_ERROR "output is not a PNG (magic ${magic})")
endif()

message(STATUS "render_fixture_smoke OK (${png_size} bytes)")
