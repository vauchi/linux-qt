# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

Name:           vauchi-qt
Version:        0.5.0
Release:        1%{?dist}
Summary:        Privacy-focused updatable contact cards (Qt desktop)
License:        GPL-3.0-or-later
URL:            https://vauchi.app

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel

# TODO: Add vauchi-cabi dependency
# Requires: libvauchi_cabi

%description
Vauchi is a privacy-focused app for exchanging updatable contact cards
in person. End-to-end encrypted and decentralized. This package provides
the native Linux desktop client built with Qt6.

%prep
# TODO: Source tarball setup

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%{_bindir}/vauchi-qt
%{_datadir}/applications/vauchi-qt.desktop
