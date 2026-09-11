# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Drives `qvauchi --render-catalog` on a two-entry screen catalog and asserts
# one valid, distinct PNG per entry and per theme/text variant. Guards the CI
# screen-catalog job (test:screen-catalog). Invoked via add_test with
# -DBIN / -DCATALOG / -DOUTPUT_DIR / -DCABI_DIR.

if(NOT EXISTS "${CATALOG}")
  message(FATAL_ERROR "catalog fixture missing: ${CATALOG}")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
set(ENV{QT_QPA_PLATFORM} offscreen)
set(ENV{LD_LIBRARY_PATH} "${CABI_DIR}:$ENV{LD_LIBRARY_PATH}")

execute_process(
  COMMAND "${BIN}" --render-catalog "${CATALOG}" "${OUTPUT_DIR}" 900 1400
  RESULT_VARIABLE rc
  ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "render-catalog exited ${rc}: ${err}")
endif()

# Mirrors the code_ids in tests/fixtures/catalog_smoke.json and the variant
# suffixes screenCatalogFileName() emits.
set(expected
  onboarding_welcome.png
  onboarding_welcome.light.png
  onboarding_welcome.large.png
  onboarding_name.png
  onboarding_name.light.png
  onboarding_name.large.png)

set(seen_hashes "")
foreach(name IN LISTS expected)
  set(png "${OUTPUT_DIR}/${name}")
  if(NOT EXISTS "${png}")
    message(FATAL_ERROR "render-catalog produced no ${name} in ${OUTPUT_DIR}")
  endif()
  file(SIZE "${png}" png_size)
  if(png_size LESS 2000)
    message(FATAL_ERROR "${name} suspiciously small (${png_size} bytes) — likely a blank frame")
  endif()
  file(READ "${png}" magic LIMIT 4 HEX)
  if(NOT magic STREQUAL "89504e47")
    message(FATAL_ERROR "${name} is not a PNG (magic ${magic})")
  endif()
  file(SHA256 "${png}" hash)
  if("${hash}" IN_LIST seen_hashes)
    message(FATAL_ERROR "${name} is byte-identical to an earlier variant — a variant or entry did not take effect")
  endif()
  list(APPEND seen_hashes "${hash}")
endforeach()

file(GLOB produced "${OUTPUT_DIR}/*.png")
list(LENGTH produced produced_count)
list(LENGTH expected expected_count)
if(NOT produced_count EQUAL expected_count)
  message(FATAL_ERROR "expected ${expected_count} PNG files, found ${produced_count}: ${produced}")
endif()

message(STATUS "render_catalog_smoke OK (${produced_count} distinct PNG files)")
