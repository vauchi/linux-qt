<!-- SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Qt display shell

This crate builds the QtWidgets renderer as a library. The app owns Core,
the hardware shell and persistent storage. The renderer accepts command
batches and reports JSON events through a callback.

Call `run_with_poll` on the GUI thread to receive an initial host poll and
another every 50 ms, even without input. Both callbacks run on that same
thread and may share thread-local state. The app drains its hardware and
engine inboxes during the poll, then calls `render`. Callbacks must return
promptly. `stop` ends the loop; callback state is released before `run`
returns. The original `run` entry point remains available without polling.

Building requires CMake, pkg-config, Qt 6 Core/Gui/Widgets/Concurrent and
libqrencode. Qt 6 DBus enables camera permission requests through the
desktop portal. No Core or GTK library is linked into this crate.

The generated layout-token header ships in `tokens/`, with its source
revision in `tokens/SOURCE`. Set `VAUCHI_TOKENS_INCLUDE` to override it.
A sibling themes checkout is not required.

Component releases use `qt-display-v<crate-version>` tags. Consumers pin
that immutable tag and the exact crate version. Tag CI tests an isolated
source archive and enforces the presentation-boundary release gate; the
legacy standalone app's distro package jobs remain manual on these tags.
