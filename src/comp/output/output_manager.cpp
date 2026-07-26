#include "comp/output/output_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::comp {

OutputManager& OutputManager::instance() noexcept {
    static OutputManager s_instance;
    return s_instance;
}

void OutputManager::add_output(const OutputConfig& config) {
    remove_output(config.name);
    m_outputs.push_back(config);
    log::info("OutputManager: Added output '{}' ({}x{}@{}Hz, scale={})",
              config.name, config.width, config.height, config.refresh_rate_mhz / 1000, config.scale);
}

void OutputManager::remove_output(const std::string& name) {
    auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                           [&name](const OutputConfig& o) { return o.name == name; });

    if (it != m_outputs.end()) {
        log::info("OutputManager: Removed output '{}'", name);
        m_outputs.erase(it);
    }
}

std::vector<OutputConfig> OutputManager::get_active_outputs() const {
    return m_outputs;
}

} // namespace tinexus::comp
