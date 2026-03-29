# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

Name:           vauchi-qt
Version:        0.5.0
Release:        1%{?dist}
Summary:        Privacy-focused updatable contact cards (Qt desktop)
License:        GPL-3.0-or-later
URL:            https://vauchi.app
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  pkg-config
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qrencode-devel
BuildRequires:  cargo
BuildRequires:  rust
BuildRequires:  git-core
BuildRequires:  qt6-qtmultimedia-devel
BuildRequires:  zbar-devel
BuildRequires:  qt6-qtconnectivity-devel
BuildRequires:  pcsc-lite-devel

Requires:       qt6-qtbase
Requires:       qrencode-libs

%description
Vauchi is a privacy-focused app for exchanging updatable contact cards
in person. End-to-end encrypted and decentralized. This package provides
the native Linux desktop client built with Qt6.

%prep
%autosetup -n %{name}-%{version}
# Clone vauchi-core for CABI build
git clone --depth 1 https://gitlab.com/vauchi/core.git vauchi-core

%build
# Build vauchi-cabi
cd vauchi-core
cargo build --release -p vauchi-cabi --features secure-storage
cd ..

%cmake -DVAUCHI_CABI_DIR=%{_builddir}/%{name}-%{version}/vauchi-core/target/release
%cmake_build

%install
%cmake_install

# Bundle libvauchi_cabi.so
install -Dm755 vauchi-core/target/release/libvauchi_cabi.so \
    %{buildroot}%{_libdir}/libvauchi_cabi.so

# Install icon
install -Dm644 resources/vauchi.svg \
    %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/vauchi-qt.svg

%files
%license LICENSE
%{_bindir}/qvauchi
%{_libdir}/libvauchi_cabi.so
%{_datadir}/applications/vauchi-qt.desktop
%{_datadir}/icons/hicolor/scalable/apps/vauchi-qt.svg
