#ifndef TINEXUS_COMP_FOCUS_MANAGER_HPP
#define TINEXUS_COMP_FOCUS_MANAGER_HPP

#include <string>
#include <cstdint>

// Forward-declare wlroots types to avoid pulling heavy C headers into every TU.
struct wlr_seat;
struct wlr_scene;
struct wlr_surface;

namespace tinexus::comp {

enum class FocusTargetType : uint8_t {
    None        = 0,
    Window      = 1,
    Launcher    = 2,
    Lockscreen  = 3,
    Notification= 4,
    PopupMenu   = 5,
    LayerSurface= 6,
};

inline const char* focus_target_to_string(FocusTargetType type) noexcept {
    switch (type) {
        case FocusTargetType::None:         return "None";
        case FocusTargetType::Window:       return "Window";
        case FocusTargetType::Launcher:     return "Launcher";
        case FocusTargetType::Lockscreen:   return "Lockscreen";
        case FocusTargetType::Notification: return "Notification";
        case FocusTargetType::PopupMenu:    return "PopupMenu";
        case FocusTargetType::LayerSurface: return "LayerSurface";
        default:                            return "Unknown";
    }
}

/// Result of a pointer surface pick.
struct PickResult {
    struct wlr_surface* surface{nullptr}; ///< null if no surface under cursor
    double              sx{0.0};          ///< cursor x relative to surface origin
    double              sy{0.0};          ///< cursor y relative to surface origin
};

/// FocusManager — central authority for both pointer AND keyboard focus.
///
/// Owns:
///   • Scene-graph hit-testing   (pick_surface via wlr_scene_node_at)
///   • Pointer focus tracking    (which surface has pointer hover)
///   • Keyboard focus tracking   (which surface has key input)
///
/// The backend calls bind() once during initialise(), then calls
/// pick_surface() on every cursor motion event.
class FocusManager {
public:
    static FocusManager& instance() noexcept;

    FocusManager()  = default;
    ~FocusManager() = default;

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /// Called once by the backend after wlr_seat and wlr_scene are ready.
    void bind(struct wlr_seat* seat, struct wlr_scene* scene) noexcept;

    // ------------------------------------------------------------------
    // Pointer focus
    // ------------------------------------------------------------------

    /// Walk the scene graph at (x, y) and return the surface + local coords.
    /// Returns PickResult with surface==nullptr if nothing found.
    [[nodiscard]] PickResult pick_surface(double x, double y) const noexcept;

    /// Update pointer focus.  Call after every cursor motion.
    /// Internally calls wlr_seat_pointer_notify_enter/motion/clear_focus.
    void update_pointer_focus(const PickResult& pick, uint32_t time_msec) noexcept;

    // ------------------------------------------------------------------
    // Keyboard focus  (expanded in Phase 4 — stubbed cleanly for now)
    // ------------------------------------------------------------------

    /// Give keyboard focus to surface.  Pass nullptr to clear.
    void set_keyboard_focus(struct wlr_surface* surface) noexcept;

    // ------------------------------------------------------------------
    // Legacy API (kept for compatibility with existing callers)
    // ------------------------------------------------------------------
    void set_focus(FocusTargetType type, uint64_t surface_id,
                   const std::string& target_id);

    [[nodiscard]] FocusTargetType   current_focus_type()   const noexcept { return m_focus_type; }
    [[nodiscard]] uint64_t          current_surface_id()   const noexcept { return m_surface_id; }
    [[nodiscard]] const std::string& current_target_id()   const noexcept { return m_target_id; }
    [[nodiscard]] struct wlr_surface* pointer_surface()    const noexcept { return m_pointer_surface; }
    [[nodiscard]] struct wlr_surface* keyboard_focus()     const noexcept { return m_keyboard_surface; }

private:
    // wlroots handles (owned by the backend, held here as non-owning ptrs)
    struct wlr_seat*    m_seat{nullptr};
    struct wlr_scene*   m_scene{nullptr};

    // Pointer state
    struct wlr_surface* m_pointer_surface{nullptr}; ///< currently hovered surface

    // Legacy keyboard-focus state
    FocusTargetType m_focus_type{FocusTargetType::None};
    uint64_t        m_surface_id{0};
    std::string     m_target_id;

    // Keyboard focus tracking (Phase 4)
    struct wlr_surface* m_keyboard_surface{nullptr}; ///< currently keyboard-focused surface
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_FOCUS_MANAGER_HPP
