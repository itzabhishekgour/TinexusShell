#include "comp/input/seat_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

bool SeatManager::bind_seat(const std::string& seat_name) {
    m_seat_name = seat_name;
    m_seat_bound = true;
    m_keymap.compile_keymap("us");
    log::info("SeatManager: Bound Wayland wl_seat capability interface '{}'", m_seat_name);
    return true;
}

void SeatManager::send_pointer_enter(uint64_t surface_id, int32_t x, int32_t y) {
    m_focused_surface_id = surface_id;
    m_pointer_x = x;
    m_pointer_y = y;
    log::info("SeatManager: Dispatched wl_pointer.enter to Surface #{} at ({},{})", surface_id, x, y);
}

void SeatManager::send_pointer_motion(int32_t x, int32_t y) {
    m_pointer_x = x;
    m_pointer_y = y;
    log::info("SeatManager: Dispatched wl_pointer.motion to ({},{})", x, y);
}

void SeatManager::send_pointer_leave(uint64_t surface_id) {
    log::info("SeatManager: Dispatched wl_pointer.leave from Surface #{}", surface_id);
    if (m_focused_surface_id == surface_id) {
        m_focused_surface_id = 0;
    }
}

void SeatManager::send_button_click(uint32_t button, uint32_t state) {
    log::info("SeatManager: Dispatched wl_pointer.button button #{} (state {})", button, state);
}

void SeatManager::send_keyboard_enter(uint64_t surface_id) {
    m_focused_surface_id = surface_id;
    log::info("SeatManager: Dispatched wl_keyboard.enter to Surface #{}", surface_id);
}

void SeatManager::send_keyboard_leave(uint64_t surface_id) {
    log::info("SeatManager: Dispatched wl_keyboard.leave from Surface #{}", surface_id);
    if (m_focused_surface_id == surface_id) {
        m_focused_surface_id = 0;
    }
}

void SeatManager::send_key_event(uint32_t key, uint32_t state) {
    uint32_t keysym = m_keymap.translate_key(key);
    log::info("SeatManager: Dispatched wl_keyboard.key #{} (keysym {}, state {})", key, keysym, state);
}

} // namespace tinexus::comp
