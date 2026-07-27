#include "comp/server/global_registry.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

GlobalRegistry::GlobalRegistry() {
    register_global("wl_compositor", 5);
    register_global("wl_shm", 1);
    register_global("wl_seat", 7);
    register_global("xdg_wm_base", 3);
    register_global("data_device_manager", 3);
}

void GlobalRegistry::register_global(const std::string& name, uint32_t version) {
    m_globals.push_back({name, version});
    log::info("GlobalRegistry: Registered Wayland interface '{}' (v{})", name, version);
}

std::vector<GlobalInterface> GlobalRegistry::globals() const {
    return m_globals;
}

bool GlobalRegistry::has_global(const std::string& name) const {
    for (const auto& g : m_globals) {
        if (g.name == name) return true;
    }
    return false;
}

} // namespace tinexus::comp
