// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Integration tests for vauchi-cabi AppEngine API.

#include "vauchi.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// --- Helper: create a temporary directory ---
static fs::path make_temp_dir() {
    auto tmpl = fs::temp_directory_path() / "vauchi-qt-test-XXXXXX";
    std::string s = tmpl.string();
    // mkdtemp modifies the template in-place
    char *result = mkdtemp(s.data());
    assert(result != nullptr);
    return fs::path(result);
}

// --- Test: create with config returns non-null ---
static void test_create_with_config() {
    auto dir = make_temp_dir();
    VauchiApp *app = vauchi_app_create_with_config(dir.c_str(), nullptr);
    assert(app != nullptr);
    vauchi_app_destroy(app);
    fs::remove_all(dir);
}

// --- Test: create with config + relay URL ---
static void test_create_with_relay() {
    auto dir = make_temp_dir();
    VauchiApp *app = vauchi_app_create_with_config(
        dir.c_str(), "wss://relay.example.com");
    assert(app != nullptr);
    vauchi_app_destroy(app);
    fs::remove_all(dir);
}

// --- Test: persistence across reopens ---
static void test_persistence() {
    auto dir = make_temp_dir();

    // First open
    VauchiApp *app1 = vauchi_app_create_with_config(dir.c_str(), nullptr);
    assert(app1 != nullptr);
    vauchi_app_destroy(app1);

    // Database file should exist
    assert(fs::exists(dir / "vauchi.db"));

    // Second open — should succeed
    VauchiApp *app2 = vauchi_app_create_with_config(dir.c_str(), nullptr);
    assert(app2 != nullptr);
    vauchi_app_destroy(app2);

    fs::remove_all(dir);
}

// --- Test: available screens starts with onboarding ---
static void test_available_screens() {
    VauchiApp *app = vauchi_app_create();
    assert(app != nullptr);

    char *json = vauchi_app_available_screens(app);
    assert(json != nullptr);
    // Should contain "onboarding" as the only screen before identity
    std::string s(json);
    assert(s.find("onboarding") != std::string::npos);
    vauchi_string_free(json);
    vauchi_app_destroy(app);
}

// --- Test: current screen returns valid JSON ---
static void test_current_screen() {
    VauchiApp *app = vauchi_app_create();
    assert(app != nullptr);

    char *json = vauchi_app_current_screen(app);
    assert(json != nullptr);
    std::string s(json);
    // Should contain screen_id field
    assert(s.find("screen_id") != std::string::npos);
    vauchi_string_free(json);
    vauchi_app_destroy(app);
}

// --- Test: handle action returns valid JSON ---
static void test_handle_action() {
    VauchiApp *app = vauchi_app_create();
    assert(app != nullptr);

    const char *action = R"({"ActionPressed":{"action_id":"create_new"}})";
    char *result = vauchi_app_handle_action(app, action);
    assert(result != nullptr);
    assert(std::strlen(result) > 0);
    vauchi_string_free(result);
    vauchi_app_destroy(app);
}

// --- Test: navigate to unknown screen doesn't crash ---
static void test_navigate_unknown() {
    VauchiApp *app = vauchi_app_create();
    assert(app != nullptr);

    char *result = vauchi_app_navigate_to(app, "nonexistent_screen");
    // May return null or error JSON, but should not crash
    if (result) vauchi_string_free(result);
    vauchi_app_destroy(app);
}

// --- Test: null handle safety ---
static void test_null_safety() {
    vauchi_app_destroy(nullptr);  // should not crash
    assert(vauchi_app_current_screen(nullptr) == nullptr);
    assert(vauchi_app_handle_action(nullptr, "{}") == nullptr);
    assert(vauchi_app_navigate_to(nullptr, "home") == nullptr);
    assert(vauchi_app_available_screens(nullptr) == nullptr);
    assert(vauchi_app_default_screen(nullptr) == nullptr);
    assert(vauchi_app_create_with_config(nullptr, nullptr) == nullptr);
}

int main() {
    test_null_safety();
    test_create_with_config();
    test_create_with_relay();
    test_persistence();
    test_available_screens();
    test_current_screen();
    test_handle_action();
    test_navigate_unknown();
    return 0;
}
