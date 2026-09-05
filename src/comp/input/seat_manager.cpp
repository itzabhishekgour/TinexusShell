#include "comp/input/seat_manager.hpp"
#include "common/logger.hpp"

extern "C" {
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>  // wlr_axis_source enum
}

namespace tinexus::comp {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool SeatManager::bind_seat(struct wlr_seat* seat, const std::string& seat_name) {
    m_seat       = seat;
    m_seat_name  = seat_name;
    m_seat_bound = (seat != nullptr);

    m_keymap.compile_keymap("us");

    log::info("[Seat] Bound wl_seat '{}' → wlr_seat={}", m_seat_name,
              static_cast<void*>(m_seat));
    return m_seat_bound;
}

// ---------------------------------------------------------------------------
// Pointer — button / axis / frame
// ---------------------------------------------------------------------------

void SeatManager::notify_button(uint32_t time_msec,
                                 uint32_t button,
                                 uint32_t state) {
    if (!m_seat) { return; }
    log::info("[Seat] pointer.button button={} state={}", button, state);
    wlr_seat_pointer_notify_button(
        m_seat, time_msec, button,
        static_cast<wl_pointer_button_state>(state));
}

void SeatManager::notify_axis(uint32_t time_msec,
                               uint32_t orientation,
                               double   delta,
                               int32_t  delta_discrete,
                               uint32_t source,
                               uint32_t relative_direction) {
    if (!m_seat) { return; }
    log::info("[Seat] pointer.axis orient={} delta={:.2f} discrete={} focused={}", 
              orientation, delta, delta_discrete, 
              static_cast<void*>(m_seat->pointer_state.focused_surface));
    wlr_seat_pointer_notify_axis(
        m_seat, time_msec,
        static_cast<wl_pointer_axis>(orientation),
        delta, delta_discrete,
        static_cast<wl_pointer_axis_source>(source),
        static_cast<wl_pointer_axis_relative_direction>(relative_direction));
    wlr_seat_pointer_notify_frame(m_seat);
}

void SeatManager::notify_frame() {
    if (!m_seat) { return; }
    wlr_seat_pointer_notify_frame(m_seat);
}

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

void SeatManager::notify_keyboard_modifiers(::wlr_keyboard* keyboard) {
    if (!m_seat || !keyboard) { return; }
    wlr_seat_set_keyboard(m_seat, keyboard);
    wlr_seat_keyboard_notify_modifiers(m_seat, &keyboard->modifiers);
}

void SeatManager::notify_keyboard_key(::wlr_keyboard* keyboard,
                                       uint32_t             time_msec,
                                       uint32_t             keycode,
                                       uint32_t             state) {
    if (!m_seat || !keyboard) { return; }
    wlr_seat_set_keyboard(m_seat, keyboard);
    wlr_seat_keyboard_notify_key(m_seat, time_msec, keycode, state);
}

} // namespace tinexus::comp
