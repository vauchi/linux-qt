// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wakeupschedule.h"

#include <algorithm>

namespace vauchi {

uint32_t wakeupDelayMillis(uint32_t earliestSecs, uint32_t deadlineSecs,
                           std::optional<uint32_t> earliestMillis) {
    const uint32_t earliest = earliestMillis.value_or(earliestSecs * 1000);
    return std::min(earliest, deadlineSecs * 1000);
}

} // namespace vauchi
