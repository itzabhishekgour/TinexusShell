#include "session/env_bootstrap.hpp"
#include "common/logger.hpp"
#include <cstdlib>
#include <iostream>

namespace tinexus::session {

EnvironmentBootstrapper& EnvironmentBootstrapper::instance() noexcept {
    static EnvironmentBootstrapper s_instance;
    return s_instance;
}

std::unordered_map<std::string, std::string> EnvironmentBootstrapper::get_environment_map() const {
    const char* uid = std::getenv("UID") ? std::getenv("UID") : "1000";
    std::string runtime_dir = "/run/user/" + std::string(uid);

    return {
        {"WAYLAND_DISPLAY", "wayland-0"},
        {"DISPLAY", ":0"},
        {"XDG_SESSION_TYPE", "wayland"},
        {"XDG_RUNTIME_DIR", runtime_dir},
        {"XDG_CURRENT_DESKTOP", "Tinexus"},
        {"XDG_SESSION_DESKTOP", "Tinexus"},
        {"XDG_DATA_DIRS", "/usr/local/share:/usr/share"},
        {"XDG_CONFIG_HOME", std::getenv("HOME") ? std::string(std::getenv("HOME")) + "/.config" : "/home/user/.config"},
        {"XDG_CACHE_HOME", std::getenv("HOME") ? std::string(std::getenv("HOME")) + "/.cache" : "/home/user/.cache"},
        {"DBUS_SESSION_BUS_ADDRESS", "unix:path=" + runtime_dir + "/bus"},
        {"LANG", "en_US.UTF-8"},
        {"LC_ALL", "en_US.UTF-8"}
    };
}

bool EnvironmentBootstrapper::bootstrap_environment() {
    log::info("EnvironmentBootstrapper: Injecting POSIX desktop environment variables...");
    auto envs = get_environment_map();
    for (const auto& [k, v] : envs) {
        setenv(k.c_str(), v.c_str(), 1);
        log::debug("Env set: {}={}", k, v);
    }
    return true;
}

void EnvironmentBootstrapper::print_environment() const {
    std::cout << "--- Tinexus Session Environment Map ---\n";
    auto envs = get_environment_map();
    for (const auto& [k, v] : envs) {
        std::cout << k << "=" << v << "\n";
    }
}

} // namespace tinexus::session
