#ifndef TXUI_CURSOR_ANIMATOR_HPP
#define TXUI_CURSOR_ANIMATOR_HPP

#include <chrono>
#include <string>

namespace txui {

// Enum representing the intended cursor shape/state
enum class CursorState {
    Arrow,
    IBeam,
    Crosshair,
    Hand,
    ResizeH,
    ResizeV
};

// Responsible for interpolating cursor shapes and positions for a premium feel
// Uses cubic easing for smooth transitions instead of instant snapping
class CursorAnimator {
public:
    CursorAnimator();

    // Set the target cursor shape we want to transition to
    void set_target_state(CursorState state);

    // Call this each frame to update the animation state
    // Returns true if an animation is currently running
    bool update(std::chrono::milliseconds delta_time);

    // Get the current morph progress (0.0 to 1.0)
    [[nodiscard]] double morph_progress() const noexcept { return m_morph_progress; }

    // Get the current state being morphed from
    [[nodiscard]] CursorState current_state() const noexcept { return m_current_state; }

    // Get the target state being morphed to
    [[nodiscard]] CursorState target_state() const noexcept { return m_target_state; }

    // Converts a CursorState to a Wayland-compatible cursor string (e.g. "left_ptr")
    static std::string state_to_wl_name(CursorState state);

private:
    CursorState m_current_state{CursorState::Arrow};
    CursorState m_target_state{CursorState::Arrow};
    
    double m_morph_progress{1.0}; // 1.0 means resting at target state
    bool m_animating{false};

    static constexpr double MORPH_SPEED = 5.0; // Morph completion speed
};

} // namespace txui

#endif // TXUI_CURSOR_ANIMATOR_HPP
