# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(vauchi-qt VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Release profile: optimized binary (matches linux-gtk Cargo.toml)
# LTO for cross-TU optimization, strip symbols, trap on overflow
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=auto")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=auto -s")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftrapv")

# --- Coverage support (gcov/lcov) ---
option(COVERAGE "Enable code coverage instrumentation (gcov/lcov)" OFF)
if(COVERAGE)
    message(STATUS "Coverage: ENABLED (gcov/lcov)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

find_package(PkgConfig REQUIRED)
pkg_check_modules(QRENCODE REQUIRED libqrencode)

# --- Optional hardware backends ---
find_package(Qt6 QUIET COMPONENTS Multimedia MultimediaWidgets)
find_package(Qt6 QUIET COMPONENTS Bluetooth)
find_package(Qt6 QUIET COMPONENTS DBus)
pkg_check_modules(ZBAR QUIET zbar)
pkg_check_modules(PCSCLITE QUIET libpcsclite)

# vauchi-cabi shared library
set(VAUCHI_CABI_DIR "${CMAKE_SOURCE_DIR}/../core/target/debug" CACHE PATH "Path to vauchi-cabi build output")
set(VAUCHI_CABI_INCLUDE "${CMAKE_SOURCE_DIR}/../core/vauchi-cabi/include" CACHE PATH "Path to vauchi-cabi headers")
find_library(VAUCHI_CABI vauchi_cabi PATHS ${VAUCHI_CABI_DIR} REQUIRED)

# Core sources
set(QVAUCHI_SOURCES
    src/main.cpp
    src/app.cpp
    src/coreui/presentationstate.cpp
    src/coreui/presentationcontroller.cpp
    src/coreui/presentationcontroller_effects.cpp
    src/coreui/presentationeffectpayload.cpp
    src/coreui/presentationsurface.cpp
    src/coreui/presentationsurface_nodes.cpp
    src/coreui/presentationsurface_qr.cpp
    src/coreui/thememanager.cpp
    src/platform/systemtray.cpp
    src/platform/menubar.cpp
    src/platform/hardwarebackend.cpp
    src/platform/directsendworker.cpp
    src/platform/screencaptureprotection.cpp
    resources/resources.qrc
)

set(QVAUCHI_LIBS Qt6::Widgets Qt6::Core ${VAUCHI_CABI} ${QRENCODE_LIBRARIES})
set(VAUCHI_TOKENS_INCLUDE "${CMAKE_SOURCE_DIR}/../themes/generated" CACHE PATH "Path to generated design tokens")
set(QVAUCHI_INCLUDES include ${QRENCODE_INCLUDE_DIRS} ${VAUCHI_CABI_INCLUDE} ${VAUCHI_TOKENS_INCLUDE})
set(QVAUCHI_DEFINES "")

# Camera backend (Qt Multimedia + ZBar)
if(Qt6Multimedia_FOUND AND Qt6MultimediaWidgets_FOUND AND ZBAR_FOUND)
    message(STATUS "Camera backend: ENABLED (Qt Multimedia + ZBar)")
    list(APPEND QVAUCHI_SOURCES src/platform/camerabackend.cpp)
    list(APPEND QVAUCHI_LIBS Qt6::Multimedia Qt6::MultimediaWidgets ${ZBAR_LIBRARIES})
    list(APPEND QVAUCHI_INCLUDES ${ZBAR_INCLUDE_DIRS})
    list(APPEND QVAUCHI_DEFINES VAUCHI_HAS_CAMERA)
else()
    message(STATUS "Camera backend: DISABLED (needs Qt6::Multimedia + zbar)")
endif()

# BLE backend (Qt Bluetooth + BlueZ D-Bus advertising)
if(Qt6Bluetooth_FOUND)
    message(STATUS "BLE backend: ENABLED (Qt Bluetooth + BlueZ D-Bus advertising)")
    list(APPEND QVAUCHI_SOURCES src/platform/blebackend.cpp src/platform/bleadvertiser.cpp)
    list(APPEND QVAUCHI_LIBS Qt6::Bluetooth Qt6::DBus)
    list(APPEND QVAUCHI_DEFINES VAUCHI_HAS_BLUETOOTH)
else()
    message(STATUS "BLE backend: DISABLED (needs Qt6::Bluetooth / qt6-connectivity)")
endif()

# Audio backend (Qt Multimedia — ultrasonic proximity)
if(Qt6Multimedia_FOUND)
    message(STATUS "Audio backend: ENABLED (Qt Multimedia)")
    list(APPEND QVAUCHI_SOURCES src/platform/audiobackend.cpp)
    list(APPEND QVAUCHI_LIBS Qt6::Multimedia)
    list(APPEND QVAUCHI_DEFINES VAUCHI_HAS_AUDIO)
else()
    message(STATUS "Audio backend: DISABLED (needs Qt6::Multimedia)")
endif()

# NFC backend (PC/SC via pcsclite — USB NFC readers)
if(PCSCLITE_FOUND)
    message(STATUS "NFC backend: ENABLED (pcsclite)")
    list(APPEND QVAUCHI_SOURCES src/platform/nfcbackend.cpp)
    list(APPEND QVAUCHI_LIBS ${PCSCLITE_LIBRARIES})
    list(APPEND QVAUCHI_INCLUDES ${PCSCLITE_INCLUDE_DIRS})
    list(APPEND QVAUCHI_DEFINES VAUCHI_HAS_NFC)
else()
    message(STATUS "NFC backend: DISABLED (needs pcsclite)")
endif()

add_executable(qvauchi ${QVAUCHI_SOURCES})
target_include_directories(qvauchi PRIVATE ${QVAUCHI_INCLUDES})
target_link_directories(qvauchi PRIVATE ${QRENCODE_LIBRARY_DIRS} ${ZBAR_LIBRARY_DIRS})
target_link_libraries(qvauchi PRIVATE ${QVAUCHI_LIBS})
target_compile_definitions(qvauchi PRIVATE ${QVAUCHI_DEFINES})
add_custom_command(
    TARGET qvauchi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f
            "${CMAKE_CURRENT_BINARY_DIR}/vauchi-qt"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_FILE_NAME:qvauchi>"
            "${CMAKE_CURRENT_BINARY_DIR}/vauchi-qt")

install(TARGETS qvauchi DESTINATION bin)
install(FILES packaging/vauchi-qt.desktop DESTINATION share/applications)

# --- Tests ---
enable_testing()

# Smoke test: CABI linkage and basic API calls
add_executable(smoke_test tests/smoke_test.cpp)
target_include_directories(smoke_test PRIVATE include)
target_link_libraries(smoke_test PRIVATE ${VAUCHI_CABI})
add_test(NAME smoke_test COMMAND smoke_test)

# AppEngine integration test: persistent storage, navigation, actions
add_executable(app_engine_test tests/app_engine_test.cpp)
target_include_directories(app_engine_test PRIVATE include)
target_link_libraries(app_engine_test PRIVATE ${VAUCHI_CABI})
add_test(NAME app_engine_test COMMAND app_engine_test)

# Core presentation reducer state: multi-surface retention and active context.
add_executable(presentation_state_test
    tests/presentation_state_test.cpp
    src/coreui/presentationstate.cpp)
target_include_directories(presentation_state_test PRIVATE src)
target_link_libraries(presentation_state_test PRIVATE Qt6::Core)
add_test(NAME presentation_state_test COMMAND presentation_state_test)

# Generic presentation surface: native controls emit only opaque protocol
# identifiers and generic values.
add_executable(presentation_surface_test
    tests/presentation_surface_test.cpp
    src/coreui/presentationsurface.cpp
    src/coreui/presentationsurface_nodes.cpp
    src/coreui/presentationsurface_qr.cpp)
target_include_directories(presentation_surface_test PRIVATE
    src ${QRENCODE_INCLUDE_DIRS})
target_link_directories(presentation_surface_test PRIVATE
    ${QRENCODE_LIBRARY_DIRS})
target_link_libraries(presentation_surface_test PRIVATE
    Qt6::Widgets Qt6::Core ${QRENCODE_LIBRARIES})
add_test(NAME presentation_surface_test COMMAND presentation_surface_test)

add_executable(presentation_controller_test
    tests/presentation_controller_test.cpp
    src/coreui/presentationcontroller.cpp
    src/coreui/presentationcontroller_effects.cpp
    src/coreui/presentationeffectpayload.cpp
    src/coreui/presentationstate.cpp
    src/coreui/presentationsurface.cpp
    src/coreui/presentationsurface_nodes.cpp
    src/coreui/presentationsurface_qr.cpp
    src/platform/hardwarebackend.cpp
    src/platform/directsendworker.cpp)
target_include_directories(presentation_controller_test PRIVATE
    include src ${VAUCHI_CABI_INCLUDE} ${QRENCODE_INCLUDE_DIRS})
target_link_directories(presentation_controller_test PRIVATE
    ${QRENCODE_LIBRARY_DIRS})
target_link_libraries(presentation_controller_test PRIVATE
    Qt6::Widgets Qt6::Core ${QRENCODE_LIBRARIES} ${VAUCHI_CABI})
add_test(NAME presentation_controller_test COMMAND presentation_controller_test)

# ThemeManager test: palette generation, stylesheet output, file loading
add_executable(theme_test tests/theme_test.cpp src/coreui/thememanager.cpp)
target_include_directories(theme_test PRIVATE include ${VAUCHI_TOKENS_INCLUDE})
target_link_libraries(theme_test PRIVATE Qt6::Widgets Qt6::Core)
add_test(NAME theme_test COMMAND theme_test)

# render-fixture catalog harness smoke: a golden fixture must render to a valid
# PNG via `qvauchi --render-fixture` (offscreen). See render_fixture_smoke.cmake.
add_test(NAME render_fixture_smoke
    COMMAND ${CMAKE_COMMAND}
        -DBIN=$<TARGET_FILE:qvauchi>
        -DFIXTURE=${CMAKE_SOURCE_DIR}/tests/fixtures/generic_surface.json
        -DPNG_OUT=${CMAKE_CURRENT_BINARY_DIR}/render_fixture_smoke.png
        -DCABI_DIR=${VAUCHI_CABI_DIR}
        -P ${CMAKE_SOURCE_DIR}/tests/render_fixture_smoke.cmake)
