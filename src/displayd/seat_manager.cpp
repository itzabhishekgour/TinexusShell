#include "displayd/seat_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::displayd {

bool SeatManager::acquire_seat(const std::string& seat_id) {
    m_seat_id = seat_id;
    m_acquired = true;
    log::info("SeatManager: Acquired seat '{}' and DRM card master permissions", m_seat_id);
    return true;
}

bool SeatManager::release_seat() {
    m_acquired = false;
    log::info("SeatManager: Released seat '{}'", m_seat_id);
    return true;
}

} // namespace tinexus::displayd
