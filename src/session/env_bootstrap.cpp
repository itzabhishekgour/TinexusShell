#include "session/env_bootstrap.hpp"
#include "common/logger.hpp"
#include <cstdlib>

namespace tinexus::session {

EnvironmentBootstrapper& EnvironmentBootstrapper::instance() noexcept {
    static EnvironmentBootstrapper s_instance;
    return s_instance;
}

bool EnvironmentBootstrapper::bootstrap_environment() {
    return EnvBootstrap::apply_environment();
}

std::unordered_map<std::string, std::string> EnvironmentBootstrapper::get_environment_map() const {
    return {
        {"XDG_SESSION_TYPE", "wayland"},
        {"XDG_CURRENT_DESKTOP", "Tinexus"},
        {"DESKTOP_SESSION", "tinexus"},
        {"WAYLAND_DISPLAY", "wayland-1"}
    };
}

void EnvironmentBootstrapper::print_environment() const {
    log::info("XDG Environment Map:");
    for (const auto& [k, v] : get_environment_map()) {
        log::info("  {}={}", k, v);
    }
}

bool EnvBootstrap::apply_environment() {
    log::info("EnvBootstrap: Exporting XDG session environment variables...");

    setenv("XDG_SESSION_TYPE", "wayland", 1);
    setenv("XDG_CURRENT_DESKTOP", "Tinexus", 1);
    setenv("DESKTOP_SESSION", "tinexus", 1);
    setenv("WAYLAND_DISPLAY", "wayland-1", 1);
    setenv("XDG_SESSION_CLASS", "user", 1);
    setenv("XDG_SESSION_DESKTOP", "tinexus", 1);

    log::info("EnvBootstrap: Environment exported (XDG_SESSION_TYPE=wayland, XDG_CURRENT_DESKTOP=Tinexus)");
    return true;
}

} // namespace tinexus::session
