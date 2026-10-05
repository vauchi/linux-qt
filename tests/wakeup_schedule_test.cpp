// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

// Core's `ScheduleWakeup` asks for a wake at `earliest_secs` (or, during a
// live QR exchange, the sub-second `earliest_millis`), capped at
// `deadline_secs`. A shell that only read whole seconds ran a ~300ms QR
// frame dwell at ~1013ms on Android
// (2026-08-18-hover-transfer-stalls-on-the-last-chunk) — the same defect
// macOS's `armWakeupTimer` fixed. Pure function, so the numbers are
// asserted without arming any real timer (vauchi/private#450).

#include "wakeupschedule.h"

#include <cassert>

int main() {
    // earliest_millis wins over earliest_secs when both are present.
    assert(vauchi::wakeupDelayMillis(30, 60, 100) == 100);

    // earliest_secs applies when earliest_millis is absent.
    assert(vauchi::wakeupDelayMillis(5, 60, std::nullopt) == 5000);

    // The deadline caps a large earliest_secs.
    assert(vauchi::wakeupDelayMillis(120, 10, std::nullopt) == 10000);

    // The deadline caps a large earliest_millis too.
    assert(vauchi::wakeupDelayMillis(1, 1, 5000) == 1000);

    // Equal to the deadline is not capped away.
    assert(vauchi::wakeupDelayMillis(10, 10, std::nullopt) == 10000);

    return 0;
}
