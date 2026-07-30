// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Smoke test: verify the generic Event/Command C ABI can be loaded and called.

#include "vauchi.h"
#include <cassert>
#include <cstring>

int main() {
    VauchiApp *app = vauchi_app_create();
    assert(app != nullptr);

    char *initial = vauchi_app_initial_commands(app);
    assert(initial != nullptr);
    assert(std::strstr(initial, "ReplaceSurface") != nullptr);
    vauchi_string_free(initial);

    const char *environment =
        "{\"PresentationEnvironmentChanged\":{\"available_width\":700,"
        "\"available_height\":900,\"input_modes\":[\"pointer\",\"keyboard\"],"
        "\"motion\":\"full\"}}";
    char *commands = vauchi_app_dispatch(app, environment);
    assert(commands != nullptr);
    assert(std::strstr(commands, "SetPresentationProfile") != nullptr);
    vauchi_string_free(commands);

    vauchi_app_destroy(app);
    vauchi_app_destroy(nullptr); // should not crash

    return 0;
}
