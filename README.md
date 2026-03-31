<!-- SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

> **Mirror:** This repo is a read-only mirror of [gitlab.com/vauchi/linux-qt](https://gitlab.com/vauchi/linux-qt). Please open issues and merge requests there.

[![Pipeline](https://img.shields.io/endpoint?url=https://vauchi.gitlab.io/linux-qt/badges/pipeline.json&label=pipeline)](https://gitlab.com/vauchi/linux-qt/-/pipelines)
[![REUSE](https://api.reuse.software/badge/gitlab.com/vauchi/linux-qt)](https://api.reuse.software/info/gitlab.com/vauchi/linux-qt)

> [!WARNING]
> **Pre-Alpha Software** - This project is under heavy development
> and not ready for production use.
> APIs may change without notice. Use at your own risk.

# Vauchi Linux Qt

Native Linux desktop app (Qt variant) for Vauchi —
privacy-focused contact card exchange.

Built with Qt6 Widgets + C++. Uses `vauchi-cabi` C ABI bindings via `QJsonDocument`.

## Prerequisites

- Qt6 development libraries (`qt6-base-dev`)
- CMake 3.16+
- C++17 compiler

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Architecture

This app implements the core-driven UI contract:

- **ScreenRenderer** renders `ScreenModel` from core (JSON via C ABI)
- **14 component widgets** map to core's `Component` enum variants
- **ActionHandler** maps user input to `UserAction` JSON
- **Platform chrome**: QSystemTrayIcon, QMenuBar

All business logic lives in `vauchi-core` (Rust). This repo is a pure rendering layer.

## Packaging

- AppImage
- .deb
- .rpm

## License

GPL-3.0-or-later
