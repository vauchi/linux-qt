# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(vauchi-qt VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

find_package(PkgConfig REQUIRED)
pkg_check_modules(QRENCODE REQUIRED libqrencode)

# --- Optional hardware backends ---
find_package(Qt6 QUIET COMPONENTS Multimedia MultimediaWidgets)
find_package(Qt6 QUIET COMPONENTS Bluetooth)
pkg_check_modules(ZBAR QUIET zbar)

# vauchi-cabi shared library
set(VAUCHI_CABI_DIR "${CMAKE_SOURCE_DIR}/../core/target/debug" CACHE PATH "Path to vauchi-cabi build output")
set(VAUCHI_CABI_INCLUDE "${CMAKE_SOURCE_DIR}/../core/vauchi-cabi/include" CACHE PATH "Path to vauchi-cabi headers")
find_library(VAUCHI_CABI vauchi_cabi PATHS ${VAUCHI_CABI_DIR} REQUIRED)

# Core sources
set(QVAUCHI_SOURCES
    src/main.cpp
    src/app.cpp
    src/coreui/screenrenderer.cpp
    src/coreui/componentrenderer.cpp
    src/coreui/components/textcomponent.cpp
    src/coreui/components/textinputcomponent.cpp
    src/coreui/components/togglelistcomponent.cpp
    src/coreui/components/fieldlistcomponent.cpp
    src/coreui/components/cardpreviewcomponent.cpp
    src/coreui/components/infopanelcomponent.cpp
    src/coreui/components/contactlistcomponent.cpp
    src/coreui/components/settingsgroupcomponent.cpp
    src/coreui/components/actionlistcomponent.cpp
    src/coreui/components/statusindicatorcomponent.cpp
    src/coreui/components/pininputcomponent.cpp
    src/coreui/components/qrcodecomponent.cpp
    src/coreui/components/confirmationdialogcomponent.cpp
    src/coreui/components/showtoastcomponent.cpp
    src/coreui/components/inlineconfirmcomponent.cpp
    src/coreui/components/editabletextcomponent.cpp
    src/coreui/components/dividercomponent.cpp
    src/platform/systemtray.cpp
    src/platform/menubar.cpp
    src/platform/hardwarebackend.cpp
    resources/resources.qrc
)

set(QVAUCHI_LIBS Qt6::Widgets Qt6::Core ${VAUCHI_CABI} ${QRENCODE_LIBRARIES})
set(QVAUCHI_INCLUDES include ${QRENCODE_INCLUDE_DIRS} ${VAUCHI_CABI_INCLUDE})
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

# BLE backend (Qt Bluetooth)
if(Qt6Bluetooth_FOUND)
    message(STATUS "BLE backend: ENABLED (Qt Bluetooth)")
    list(APPEND QVAUCHI_SOURCES src/platform/blebackend.cpp)
    list(APPEND QVAUCHI_LIBS Qt6::Bluetooth)
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

add_executable(qvauchi ${QVAUCHI_SOURCES})
target_include_directories(qvauchi PRIVATE ${QVAUCHI_INCLUDES})
target_link_libraries(qvauchi PRIVATE ${QVAUCHI_LIBS})
target_compile_definitions(qvauchi PRIVATE ${QVAUCHI_DEFINES})

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
