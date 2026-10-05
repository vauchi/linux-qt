// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>

namespace vauchi {

/// Delay in milliseconds before Core's next `ScheduleWakeup` should fire.
/// `earliestMillis` wins when present — a live QR exchange asks for
/// wakes every ~100ms, which whole seconds cannot express; otherwise
/// `earliestSecs` applies. Either way the result is capped by
/// `deadlineSecs`, so a stray large earliest value cannot stall the loop
/// (vauchi/private#450, as on macOS's `armWakeupTimer`).
uint32_t wakeupDelayMillis(uint32_t earliestSecs, uint32_t deadlineSecs,
                           std::optional<uint32_t> earliestMillis);

} // namespace vauchi
