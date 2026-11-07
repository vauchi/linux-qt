# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Exercise action selection at the AT-SPI boundary."""

import unittest

import helpers


class NativeActions:
    def __init__(self, names):
        self.names = names
        self.outcomes = []

    def get_n_actions(self):
        return len(self.names)

    def get_action_name(self, index):
        return self.names[index]

    def do_action(self, index):
        self.outcomes.append(self.names[index])
        return True


class NativeWidget:
    def __init__(self, actions):
        self.actions = actions

    def get_role_name(self):
        return "button"

    def get_name(self):
        return "Settings"

    def get_child_count(self):
        return 0

    def get_action_iface(self):
        return self.actions


class ActionDispatchTests(unittest.TestCase):
    # @internal — Qt's checkable buttons expose Toggle before Press (#1109).
    def test_click_activates_instead_of_only_toggling_selection(self):
        actions = NativeActions(["Toggle", "Press", "Set Focus"])
        self.assertTrue(helpers.click_button(NativeWidget(actions), "Settings"))
        self.assertEqual(actions.outcomes, ["Press"])

    # @internal — native action order and spelling do not define the gesture.
    def test_supported_activation_names_work_at_any_index(self):
        for name in ("Press", "click", "Activate"):
            with self.subTest(name=name):
                actions = NativeActions(["Set Focus", name])
                self.assertTrue(helpers.invoke_action(NativeWidget(actions)))
                self.assertEqual(actions.outcomes, [name])

    # @internal — a focus-only action must not be reported as activation.
    def test_focus_only_widget_is_not_activated(self):
        actions = NativeActions(["Set Focus"])
        self.assertFalse(helpers.invoke_action(NativeWidget(actions)))
        self.assertEqual(actions.outcomes, [])

    # @internal — toggles with no Press action retain their native gesture.
    def test_toggle_only_control_still_changes_its_value(self):
        actions = NativeActions(["Toggle"])
        self.assertTrue(helpers.invoke_action(NativeWidget(actions)))
        self.assertEqual(actions.outcomes, ["Toggle"])


if __name__ == "__main__":
    unittest.main()
