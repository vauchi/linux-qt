// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Integration tests for vauchi-cabi AppEngine API.

#include "vauchi.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>

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

// --- Test: canonical presentation reducer boundary ---
static void test_presentation_reducer() {
    VauchiApp *app = vauchi_app_create();
    assert(app != nullptr);

    char *initial = vauchi_app_initial_commands(app);
    assert(initial != nullptr);
    std::string initialJson(initial);
    assert(initialJson.find("ReplaceSurface") != std::string::npos);
    assert(initialJson.find("SetContextBar") != std::string::npos);
    vauchi_string_free(initial);

    const char *environment =
        R"({"PresentationEnvironmentChanged":{"available_width":840,"available_height":700,"input_modes":["pointer","keyboard"],"motion":"reduced"}})";
    char *commands = vauchi_app_dispatch(app, environment);
    assert(commands != nullptr);
    std::string commandJson(commands);
    assert(commandJson.find("SetPresentationProfile") != std::string::npos);
    assert(commandJson.find("\"expanded\"") != std::string::npos);
    vauchi_string_free(commands);
    vauchi_app_destroy(app);
}

// --- Test: create with keyring (may fall back to config-only on macOS/CI) ---
static void test_create_with_keyring() {
    auto dir = make_temp_dir();
    // On Linux with D-Bus Secret Service, this uses the keyring.
    // On macOS or CI without a keyring, it may return null (expected).
    VauchiApp *app = vauchi_app_create_with_keyring(dir.c_str(), nullptr);
    if (app) {
        char *commands = vauchi_app_initial_commands(app);
        assert(commands != nullptr);
        assert(std::strlen(commands) > 0);
        vauchi_string_free(commands);
        vauchi_app_destroy(app);
    }
    fs::remove_all(dir);
}

// --- Test: create with keyring null data_dir returns null ---
static void test_create_with_keyring_null_dir() {
    VauchiApp *app = vauchi_app_create_with_keyring(nullptr, nullptr);
    assert(app == nullptr);
}

// --- Test: on_wakeup returns a valid notifications+commands envelope ---
static void test_on_wakeup() {
    VauchiApp *app = vauchi_app_create();
    assert(app != nullptr);

    char *json = vauchi_app_on_wakeup(app);
    assert(json != nullptr);
    std::string s(json);
    assert(s.find("\"notifications\"") != std::string::npos);
    assert(s.find("\"commands\"") != std::string::npos);
    vauchi_string_free(json);
    vauchi_app_destroy(app);
}

// --- Test: null handle safety ---
static void test_null_safety() {
    vauchi_app_destroy(nullptr);  // should not crash
    assert(vauchi_app_initial_commands(nullptr) == nullptr);
    assert(vauchi_app_dispatch(nullptr, "{}") == nullptr);
    assert(vauchi_app_create_with_config(nullptr, nullptr) == nullptr);
    assert(vauchi_app_on_wakeup(nullptr) == nullptr);
}

int main() {
    test_null_safety();
    test_create_with_config();
    test_create_with_relay();
    test_persistence();
    test_presentation_reducer();
    test_create_with_keyring();
    test_create_with_keyring_null_dir();
    test_on_wakeup();
    return 0;
}
