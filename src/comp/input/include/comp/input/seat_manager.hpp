#ifndef TINEXUS_COMP_SEAT_MANAGER_HPP
#define TINEXUS_COMP_SEAT_MANAGER_HPP

#include "comp/input/keymap_engine.hpp"
#include "comp/input/interaction_controller.hpp"
#include <string>
#include <cstdint>

// Forward-declare wlroots types at global scope (NOT inside any namespace).
// Placing these inside tinexus::comp would create tinexus::comp::wlr_* types
// that cannot be implicitly converted to the global ::wlr_* types.
struct wlr_seat;
struct wlr_keyboard;

namespace tinexus::comp {

/// SeatManager — owns the wlr_seat* handle and dispatches low-level
/// Wayland pointer / keyboard seat events to connected clients.
///
/// Responsibilities:
///   • Hold the wlr_seat* handle (bound once at compositor init)
///   • button, axis, frame seat notifications  (pointer path)
///   • keyboard modifier / key seat notifications
///
/// NOT responsible for:
///   • Surface hit-testing (→ FocusManager::pick_surface)
///   • notify_enter / notify_motion (→ FocusManager::update_pointer_focus)
class SeatManager {
public:
    static SeatManager& instance() {
        static SeatManager inst;
        return inst;
    }

    SeatManager(const SeatManager&)             = delete;
    SeatManager& operator=(const SeatManager&)  = delete;

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /// Called once by the backend after wlr_seat is created.
    /// seat_name is stored for logging; actual wlr_seat* must also be passed
    /// so SeatManager can forward seat events.
    bool bind_seat(struct wlr_seat* seat, const std::string& seat_name = "seat0");

    // ------------------------------------------------------------------
    // Pointer — button / axis / frame
    // (enter + motion are handled by FocusManager)
    // ------------------------------------------------------------------
    void notify_button(uint32_t time_msec, uint32_t button, uint32_t state);
    void notify_axis(uint32_t time_msec, uint32_t orientation,
                     double delta, int32_t delta_discrete,
                     uint32_t source, uint32_t relative_direction);
    void notify_frame();

    // ------------------------------------------------------------------
    // Keyboard
    // ------------------------------------------------------------------
    void notify_keyboard_modifiers(::wlr_keyboard* keyboard);
    void notify_keyboard_key(::wlr_keyboard* keyboard,
                              uint32_t time_msec, uint32_t keycode, uint32_t state);

    // ------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------
    [[nodiscard]] KeymapEngine&         keymap()             noexcept { return m_keymap; }
    [[nodiscard]] InteractionController& interaction()       noexcept { return m_interaction; }
    [[nodiscard]] bool                  seat_bound()   const noexcept { return m_seat_bound; }
    [[nodiscard]] struct wlr_seat*      seat()         const noexcept { return m_seat; }

private:
    SeatManager()  = default;
    ~SeatManager() = default;

    struct wlr_seat* m_seat{nullptr};
    std::string      m_seat_name{"seat0"};
    bool             m_seat_bound{false};

    KeymapEngine          m_keymap;
    InteractionController m_interaction;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SEAT_MANAGER_HPP
