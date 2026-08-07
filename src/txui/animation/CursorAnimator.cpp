#include <txui/animation/CursorAnimator.hpp>
#include <cmath>
#include <algorithm>

namespace txui {

CursorAnimator::CursorAnimator() = default;

void CursorAnimator::set_target_state(CursorState state) {
    if (m_target_state == state) return;

    // If we're already animating, we morph from the current progress
    // Back to 0 for a new transition
    if (m_animating) {
        m_current_state = m_target_state;
        // In a real sophisticated animator, you'd blend the shapes dynamically.
        // For v0.1 we use a cubic alpha/scale transition.
    } else {
        m_current_state = m_target_state;
    }
    
    m_target_state = state;
    m_morph_progress = 0.0;
    m_animating = true;
}

bool CursorAnimator::update(std::chrono::milliseconds delta_time) {
    if (!m_animating) return false;

    double dt = static_cast<double>(delta_time.count()) / 1000.0;
    
    m_morph_progress += MORPH_SPEED * dt;
    if (m_morph_progress >= 1.0) {
        m_morph_progress = 1.0;
        m_current_state = m_target_state;
        m_animating = false;
    }
    
    return m_animating;
}

std::string CursorAnimator::state_to_wl_name(CursorState state) {
    switch (state) {
        case CursorState::Arrow:   return "left_ptr";
        case CursorState::IBeam:   return "xterm";
        case CursorState::Crosshair: return "crosshair";
        case CursorState::Hand:    return "hand2";
        case CursorState::ResizeH: return "ew-resize";
        case CursorState::ResizeV: return "ns-resize";
        default:                   return "left_ptr";
    }
}

} // namespace txui
