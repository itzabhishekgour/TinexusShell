#ifndef TINEXUS_COMP_ANIMATION_MANAGER_HPP
#define TINEXUS_COMP_ANIMATION_MANAGER_HPP

#include "comp/animation/animation.hpp"
#include "comp/window/window_state.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cmath>

namespace tinexus::comp {

enum class WindowAnimationType : uint8_t {
    None,
    Open,
    Close,
    Minimize,
    Restore,
    Maximize,
    Unmaximize,
    Snap,
    FocusTransition
};

inline const char* to_string(WindowAnimationType type) noexcept {
    switch (type) {
        case WindowAnimationType::None: return "None";
        case WindowAnimationType::Open: return "Open";
        case WindowAnimationType::Close: return "Close";
        case WindowAnimationType::Minimize: return "Minimize";
        case WindowAnimationType::Restore: return "Restore";
        case WindowAnimationType::Maximize: return "Maximize";
        case WindowAnimationType::Unmaximize: return "Unmaximize";
        case WindowAnimationType::Snap: return "Snap";
        case WindowAnimationType::FocusTransition: return "FocusTransition";
    }
    return "Unknown";
}

struct WindowAnimation {
    uint64_t window_id{0};
    void* target_handle{nullptr};
    WindowAnimationType type{WindowAnimationType::None};
    AnimationCurve curve{AnimationCurve::EaseDecelerate};

    double elapsed_sec{0.0};
    double duration_sec{0.150}; // 150ms default snappy desktop transition

    // Geometry interpolation
    WindowBox start_geom{};
    WindowBox target_geom{};
    WindowBox current_geom{};

    // Opacity
    double start_opacity{1.0};
    double target_opacity{1.0};
    double current_opacity{1.0};

    // Scale
    double start_scale{1.0};
    double target_scale{1.0};
    double current_scale{1.0};

    // Callbacks
    std::function<void(const WindowAnimation&)> on_step;
    std::function<void(const WindowAnimation&)> on_complete;

    bool finished{false};
};

class AnimationManager {
public:
    static AnimationManager& instance() noexcept;

    AnimationManager() = default;
    ~AnimationManager() = default;

    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;

    void set_frame_scheduler(std::function<void()> schedule_frame) noexcept {
        m_schedule_frame = std::move(schedule_frame);
    }

    void start_animation(WindowAnimation anim);
    void cancel_animation(uint64_t window_id) noexcept;
    void cancel_all() noexcept;

    [[nodiscard]] bool has_active_animations() const noexcept {
        return !m_active_animations.empty();
    }

    [[nodiscard]] size_t active_animation_count() const noexcept {
        return m_active_animations.size();
    }

    [[nodiscard]] bool has_animation(uint64_t window_id) const noexcept {
        return m_active_animations.find(window_id) != m_active_animations.end();
    }

    const WindowAnimation* get_animation(uint64_t window_id) const noexcept;

    void tick(double dt);

    static double interpolate(double t, AnimationCurve curve) noexcept;

private:
    std::unordered_map<uint64_t, WindowAnimation> m_active_animations;
    std::function<void()> m_schedule_frame;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_ANIMATION_MANAGER_HPP
