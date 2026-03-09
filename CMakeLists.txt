# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(vauchi-qt VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

# vauchi-cabi shared library
set(VAUCHI_CABI_DIR "" CACHE PATH "Path to vauchi-cabi build output")
find_library(VAUCHI_CABI vauchi_cabi PATHS ${VAUCHI_CABI_DIR} REQUIRED)

add_executable(vauchi-qt
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
    src/coreui/components/dividercomponent.cpp
    src/platform/systemtray.cpp
    src/platform/menubar.cpp
)

target_include_directories(vauchi-qt PRIVATE include)
target_link_libraries(vauchi-qt PRIVATE Qt6::Widgets Qt6::Core ${VAUCHI_CABI})

install(TARGETS vauchi-qt DESTINATION bin)
install(FILES packaging/vauchi-qt.desktop DESTINATION share/applications)
