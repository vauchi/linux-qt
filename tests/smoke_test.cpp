// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Smoke test: verify vauchi-cabi can be loaded and called.

#include "vauchi.h"
#include <cassert>
#include <cstring>

int main() {
    // Create an onboarding workflow
    VauchiWorkflow *wf = vauchi_workflow_create("onboarding");
    assert(wf != nullptr);

    // Get the current screen JSON
    char *screen = vauchi_workflow_current_screen(wf);
    assert(screen != nullptr);
    assert(std::strlen(screen) > 0);
    vauchi_string_free(screen);

    // Unknown workflow type returns null
    VauchiWorkflow *bad = vauchi_workflow_create("nonexistent");
    assert(bad == nullptr);

    // Null input returns null
    VauchiWorkflow *null_wf = vauchi_workflow_create(nullptr);
    assert(null_wf == nullptr);

    // Clean up
    vauchi_workflow_destroy(wf);
    vauchi_workflow_destroy(nullptr); // should not crash

    return 0;
}
