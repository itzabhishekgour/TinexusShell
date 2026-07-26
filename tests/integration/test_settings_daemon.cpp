#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "settings/config_store.hpp"
#include "settings/schema_validator.hpp"
#include "settings/settings_daemon.hpp"

void test_schema_validation() {
    assert(tinexus::settings::SchemaValidator::validate_theme("dark"));
    assert(tinexus::settings::SchemaValidator::validate_theme("light"));
    assert(!tinexus::settings::SchemaValidator::validate_theme("invalid_theme"));

    assert(tinexus::settings::SchemaValidator::validate_scale(1.25f));
    assert(!tinexus::settings::SchemaValidator::validate_scale(99.0f));

    std::cout << "[PASS] test_schema_validation\n";
}

void test_atomic_config_store() {
    auto config_path = std::filesystem::current_path() / "test_settings.toml";
    auto& store = tinexus::settings::ConfigStore::instance();

    assert(store.save_settings_atomic(config_path));
    assert(std::filesystem::exists(config_path));

    assert(store.load_settings(config_path));
    assert(store.get_settings().theme == "dark");

    std::filesystem::remove(config_path);
    std::cout << "[PASS] test_atomic_config_store\n";
}

void test_settings_daemon_updates() {
    auto& daemon = tinexus::settings::SettingsDaemon::instance();
    auto config_path = std::filesystem::current_path() / "test_daemon_settings.toml";
    assert(daemon.initialize(config_path));

    assert(daemon.update_theme("light"));
    assert(!daemon.update_theme("invalid_theme"));

    assert(daemon.update_scale(1.5f));
    assert(!daemon.update_scale(50.0f));

    std::filesystem::remove(config_path);
    std::cout << "[PASS] test_settings_daemon_updates\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_settings");
    tinexus::log::info("Running Integration Test Suite for Settings Daemon (tinexus-settings)...");

    test_schema_validation();
    test_atomic_config_store();
    test_settings_daemon_updates();

    tinexus::log::info("All Settings Daemon integration tests passed 100%!");
    return 0;
}
