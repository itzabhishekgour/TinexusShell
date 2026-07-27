#include "comp/server/protocol_dispatcher.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

ProtocolDispatcher& ProtocolDispatcher::instance() noexcept {
    static ProtocolDispatcher s_instance;
    return s_instance;
}

bool ProtocolDispatcher::register_module(std::shared_ptr<ProtocolModule> module) {
    if (!module) return false;
    std::string name = module->name();
    if (m_modules.find(name) != m_modules.end()) {
        log::error("ProtocolModule '{}' already registered", name);
        return false;
    }
    m_modules[name] = module;
    m_order.push_back(name);
    log::info("Registered ProtocolModule '{}'", name);
    return true;
}

std::shared_ptr<ProtocolModule> ProtocolDispatcher::get_module(const std::string& name) const {
    auto it = m_modules.find(name);
    if (it != m_modules.end()) return it->second;
    return nullptr;
}

bool ProtocolDispatcher::has_module(const std::string& name) const {
    return m_modules.find(name) != m_modules.end();
}

bool ProtocolDispatcher::init_all(struct wl_display* display) {
    bool ok = true;
    for (const auto& name : m_order) {
        auto module = m_modules[name];
        if (module) {
            bool success = module->register_globals(display);
            log::info("Initialized ProtocolModule '{}': {}", name, success ? "SUCCESS" : "FAILED");
            if (!success) ok = false;
        }
    }
    return ok;
}

void ProtocolDispatcher::destroy_all() {
    for (auto it = m_order.rbegin(); it != m_order.rend(); ++it) {
        auto module = m_modules[*it];
        if (module) {
            module->destroy();
            log::info("Destroyed ProtocolModule '{}'", *it);
        }
    }
    m_modules.clear();
    m_order.clear();
}

std::vector<std::string> ProtocolDispatcher::registered_modules() const {
    return m_order;
}

} // namespace tinexus::comp
