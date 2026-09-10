#include "comp/animation/animation_manager.hpp"
#include <algorithm>

namespace tinexus::comp {

AnimationManager& AnimationManager::instance() noexcept {
    static AnimationManager s_instance;
    return s_instance;
}

void AnimationManager::start_animation(WindowAnimation anim) {
    uint64_t id = anim.window_id;
    anim.current_geom = anim.start_geom;
    anim.current_opacity = anim.start_opacity;
    anim.current_scale = anim.start_scale;
    anim.elapsed_sec = 0.0;
    anim.finished = false;

    m_active_animations[id] = std::move(anim);

    if (m_schedule_frame) {
        m_schedule_frame();
    }
}

void AnimationManager::cancel_animation(uint64_t window_id) noexcept {
    m_active_animations.erase(window_id);
}

void AnimationManager::cancel_all() noexcept {
    m_active_animations.clear();
}

const WindowAnimation* AnimationManager::get_animation(uint64_t window_id) const noexcept {
    auto it = m_active_animations.find(window_id);
    return it != m_active_animations.end() ? &it->second : nullptr;
}

void AnimationManager::tick(double dt) {
    if (m_active_animations.empty() || dt < 0.0) return;

    std::vector<uint64_t> completed_ids;
    std::vector<std::function<void(const WindowAnimation&)>> completion_callbacks;
    std::vector<WindowAnimation> completed_anims;

    for (auto& [id, anim] : m_active_animations) {
        anim.elapsed_sec += dt;
        double progress = anim.duration_sec > 0.0 ? (anim.elapsed_sec / anim.duration_sec) : 1.0;

        if (progress >= 1.0) {
            // Snaps exactly to final target state
            anim.finished = true;
            anim.current_geom = anim.target_geom;
            anim.current_opacity = anim.target_opacity;
            anim.current_scale = anim.target_scale;

            if (anim.on_step) {
                anim.on_step(anim);
            }

            completed_ids.push_back(id);
            if (anim.on_complete) {
                completion_callbacks.push_back(anim.on_complete);
                completed_anims.push_back(anim);
            }
        } else {
            double t = interpolate(progress, anim.curve);

            anim.current_geom.x = anim.start_geom.x + static_cast<int32_t>(std::round((anim.target_geom.x - anim.start_geom.x) * t));
            anim.current_geom.y = anim.start_geom.y + static_cast<int32_t>(std::round((anim.target_geom.y - anim.start_geom.y) * t));
            anim.current_geom.width = anim.start_geom.width + static_cast<int32_t>(std::round((anim.target_geom.width - anim.start_geom.width) * t));
            anim.current_geom.height = anim.start_geom.height + static_cast<int32_t>(std::round((anim.target_geom.height - anim.start_geom.height) * t));

            anim.current_opacity = anim.start_opacity + (anim.target_opacity - anim.start_opacity) * t;
            anim.current_scale = anim.start_scale + (anim.target_scale - anim.start_scale) * t;

            if (anim.on_step) {
                anim.on_step(anim);
            }
        }
    }

    // Erase completed animations first so has_active_animations() reflects post-completion status
    for (uint64_t id : completed_ids) {
        m_active_animations.erase(id);
    }

    // Execute complete callbacks
    for (size_t i = 0; i < completion_callbacks.size(); ++i) {
        completion_callbacks[i](completed_anims[i]);
    }
}

double AnimationManager::interpolate(double t, AnimationCurve curve) noexcept {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;

    switch (curve) {
        case AnimationCurve::Linear:
            return t;
        case AnimationCurve::EaseInCubic:
            return t * t * t;
        case AnimationCurve::EaseOutCubic: {
            double p = t - 1.0;
            return p * p * p + 1.0;
        }
        case AnimationCurve::EaseInOutCubic:
            return (t < 0.5) ? (4.0 * t * t * t)
                             : (1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0);
        case AnimationCurve::EaseDecelerate: {
            double p = t - 1.0;
            return p * p * p * p * p + 1.0;
        }
        case AnimationCurve::EaseAccelerate:
            return t * t * t * t;
        case AnimationCurve::EaseSpring: {
            if (t < 0.7) {
                return 2.2 * t * t;
            } else {
                double u = t - 1.0;
                return 1.0 + 0.15 * u * u * (3.0 + 2.0 * u);
            }
        }
    }
    return t;
}

} // namespace tinexus::comp
