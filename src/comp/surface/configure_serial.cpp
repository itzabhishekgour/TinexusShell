#include "comp/surface/configure_serial.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

uint32_t ConfigureSerialManager::generate() {
    uint32_t serial = ++m_next_serial;
    m_pending_serials.insert(serial);
    log::info("ConfigureSerialManager: Generated configure serial token #{}", serial);
    return serial;
}

bool ConfigureSerialManager::validate(uint32_t serial) {
    auto it = m_pending_serials.find(serial);
    if (it != m_pending_serials.end()) {
        m_pending_serials.erase(it);
        log::info("ConfigureSerialManager: Validated client ack for configure serial token #{}", serial);
        return true;
    }
    log::error("ConfigureSerialManager: Rejected invalid or expired configure serial token #{}!", serial);
    return false;
}

void ConfigureSerialManager::expire(uint32_t serial) {
    m_pending_serials.erase(serial);
}

size_t ConfigureSerialManager::pending_count() const noexcept {
    return m_pending_serials.size();
}

} // namespace tinexus::comp
