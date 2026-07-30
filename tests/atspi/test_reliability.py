# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Live Linux Qt reliability scenarios shared with the GTK pass."""

from helpers import (
    click_button,
    dump_tree,
    find_all,
    find_app,
    find_one,
    set_text,
    wait_for_element,
)


def _refresh_app(proc):
    app = find_app("vauchi", timeout=5.0, pid=proc.pid)
    assert app is not None
    return app


def test_onboarding_identity_survives_restart(qt_app_restartable):
    """Create an identity through AT-SPI, restart, and recover it."""
    first_proc, app = qt_app_restartable()

    labels = [
        label.get_name()
        for label in find_all(app, role="label")
        if label.get_name()
    ]
    assert not any(label.startswith("Missing:") for label in labels)

    assert click_button(app, "Create new identity")
    app = _refresh_app(first_proc)
    assert wait_for_element(
        app, role="text", name="Display name input", timeout=5.0
    )
    edited = set_text(app, "Display name input", "Qt Harness Explorer")
    process_error = ""
    if first_proc.poll() is not None:
        _, stderr = first_proc.communicate(timeout=5)
        process_error = stderr.decode(errors="replace")[:1000]
    assert edited, (
        f"process exit={first_proc.poll()}, stderr={process_error}\n"
        f"{dump_tree(app, max_depth=10)}"
    )
    assert wait_for_element(
        app, role="button", name="Continue", timeout=5.0
    ), dump_tree(app, max_depth=10)
    assert click_button(app, "Continue")
    app = _refresh_app(first_proc)
    assert wait_for_element(
        app, role="label", name="Choose your groups", timeout=5.0
    )
    assert click_button(app, "Continue")
    app = _refresh_app(first_proc)
    assert wait_for_element(
        app, role="label", name="Add contact info", timeout=5.0
    )
    assert click_button(app, "Continue")
    app = _refresh_app(first_proc)
    assert wait_for_element(
        app, role="label", name="What would you like to do?", timeout=5.0
    )
    assert click_button(app, "Start using the app")
    app = _refresh_app(first_proc)
    assert wait_for_element(
        app, role="label", name="Qt Harness Explorer", timeout=5.0
    ), dump_tree(app, max_depth=10)

    first_proc.terminate()
    first_proc.wait(timeout=5)

    _, reopened = qt_app_restartable()
    assert find_one(reopened, role="button", name="Create new identity") is None
    assert wait_for_element(
        reopened, role="label", name="Qt Harness Explorer", timeout=5.0
    )
