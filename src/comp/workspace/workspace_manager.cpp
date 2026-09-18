#include "comp/workspace/workspace_manager.hpp"
#include "comp/output/output_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <cmath>

extern "C" {
#define static
#include <wlr/types/wlr_scene.h>
#undef static
}

namespace tinexus::comp {

WorkspaceManager& WorkspaceManager::instance() noexcept {
    static WorkspaceManager s_instance;
    return s_instance;
}

WorkspaceManager::WorkspaceManager() {
    initialize_default_workspaces(9);
}

void WorkspaceManager::initialize_default_workspaces(uint32_t count) {
    m_workspaces.clear();
    m_workspaces.reserve(count);

    for (uint32_t i = 1; i <= count; ++i) {
        m_workspaces.push_back(Workspace{i, "Workspace " + std::to_string(i), (i == 1), {}, nullptr});
    }
    m_active_id = 1;
    m_slide_spring.reset(0.0, 0.0);
    m_is_sliding = false;
}

uint32_t WorkspaceManager::active_workspace_id() const noexcept {
    return m_active_id;
}

bool WorkspaceManager::switch_workspace(uint32_t workspace_id) {
    if (workspace_id == m_active_id && !m_is_sliding) return true;

    auto it = std::find_if(m_workspaces.begin(), m_workspaces.end(),
                           [workspace_id](const Workspace& ws) { return ws.id == workspace_id; });

    if (it == m_workspaces.end()) {
        log::warn("Attempted to switch to invalid workspace ID: {}", workspace_id);
        return false;
    }

    uint32_t from_id = m_active_id;
    uint32_t to_id = workspace_id;

    // Enable scene trees involved in the transition (source, target, and intermediates)
    uint32_t min_id = std::min(from_id, to_id);
    uint32_t max_id = std::max(from_id, to_id);
    for (auto& ws : m_workspaces) {
        if (ws.scene_tree) {
            if (ws.id >= min_id && ws.id <= max_id) {
                wlr_scene_node_set_enabled(&ws.scene_tree->node, true);
            }
        }
        ws.is_active = (ws.id == workspace_id);
    }

    m_active_id = workspace_id;

    // Set spring target smoothly without wiping velocity (enables fluid mid-flight redirection)
    double target_offset = static_cast<double>(workspace_id - 1) * static_cast<double>(m_viewport_width);
    m_slide_spring.target = target_offset;
    m_is_sliding = true;

    // Request immediate frame schedule from compositor
    if (m_frame_scheduler) {
        m_frame_scheduler();
    }

    log::info("Switched to Workspace {} (target_offset={})", workspace_id, target_offset);
    return true;
}

void WorkspaceManager::tick_animation(double dt) noexcept {
    if (!m_is_sliding) return;

    // Critically damped spring step: stiffness=380.0, damping=28.0
    bool settled = m_slide_spring.step(dt, 380.0, 28.0);

    // Update position of all workspace trees
    for (auto& ws : m_workspaces) {
        if (!ws.scene_tree) continue;
        double base_x = static_cast<double>(ws.id - 1) * static_cast<double>(m_viewport_width);
        int32_t current_x = static_cast<int32_t>(std::round(base_x - m_slide_spring.value));
        wlr_scene_node_set_position(&ws.scene_tree->node, current_x, 0);
    }

    if (settled) {
        m_is_sliding = false;
        // Off-screen workspaces disabled to eliminate redundant damage & cursor checks
        for (auto& ws : m_workspaces) {
            if (ws.scene_tree && ws.id != m_active_id) {
                wlr_scene_node_set_enabled(&ws.scene_tree->node, false);
            }
        }
        log::info("Workspace slide settled on Workspace {}", m_active_id);
    }
}

void WorkspaceManager::begin_gesture_swipe() noexcept {
    if (m_in_gesture) return;

    m_in_gesture = true;
    m_is_sliding = false; // Halt any active spring animation

    // Determine base workspace closest to current visual position
    double cur_val = m_slide_spring.value;
    double vw = static_cast<double>(m_viewport_width > 0 ? m_viewport_width : 1920);
    int nearest_idx = static_cast<int>(std::round(cur_val / vw)) + 1;
    m_gesture_start_ws = static_cast<uint32_t>(std::clamp<int>(nearest_idx, 1, static_cast<int>(m_workspaces.size())));

    m_gesture_accum_dx = 0.0;
    m_gesture_last_time = 0;
    m_gesture_velocity = 0.0;

    // Enable adjacent scene trees to ensure seamless rendering during drag
    uint32_t min_id = (m_gesture_start_ws > 1) ? m_gesture_start_ws - 1 : 1;
    uint32_t max_id = std::min<uint32_t>(m_gesture_start_ws + 1, static_cast<uint32_t>(m_workspaces.size()));
    for (auto& ws : m_workspaces) {
        if (ws.scene_tree && ws.id >= min_id && ws.id <= max_id) {
            wlr_scene_node_set_enabled(&ws.scene_tree->node, true);
        }
    }

    log::info("[Gesture] WorkspaceManager: Swipe begin (start_ws={}, current_offset={:.1f})",
              m_gesture_start_ws, cur_val);
}

void WorkspaceManager::update_gesture_swipe(double dx, uint32_t time_msec) noexcept {
    if (!m_in_gesture) return;

    double vw = static_cast<double>(m_viewport_width > 0 ? m_viewport_width : 1920);

    // Compute velocity (dt in seconds)
    if (m_gesture_last_time > 0 && time_msec > m_gesture_last_time) {
        double dt = static_cast<double>(time_msec - m_gesture_last_time) / 1000.0;
        if (dt > 0.001) {
            // Natural 1:1: moving fingers left (dx < 0) moves camera to the right (-dx > 0)
            double instant_v = (-dx) / dt;
            // Low-pass filter (70% instant, 30% previous)
            m_gesture_velocity = (m_gesture_velocity * 0.3) + (instant_v * 0.7);
        }
    }
    m_gesture_last_time = time_msec;
    m_gesture_accum_dx += dx;

    // Natural 1:1 mapping: fingers moving left (dx < 0) slides to next workspace on right (+delta)
    double delta_offset = -dx;
    double max_offset = static_cast<double>(m_workspaces.size() - 1) * vw;

    // Rubber-banding resistance when overshooting outer bounds
    if ((m_slide_spring.value < 0.0 && delta_offset < 0.0) ||
        (m_slide_spring.value > max_offset && delta_offset > 0.0)) {
        delta_offset *= 0.35; // 0.35x resistance
    }

    m_slide_spring.value += delta_offset;

    // Elastic limit clamp: 25% of viewport width
    double elastic_limit = vw * 0.25;
    m_slide_spring.value = std::clamp(m_slide_spring.value, -elastic_limit, max_offset + elastic_limit);

    // Reposition all workspace scene trees
    double cur_val = m_slide_spring.value;
    int left_idx = static_cast<int>(std::floor(cur_val / vw)) + 1;
    int right_idx = left_idx + 1;

    for (auto& ws : m_workspaces) {
        if (!ws.scene_tree) continue;

        // Dynamically enable trees within visible range
        bool is_near = (static_cast<int>(ws.id) >= left_idx - 1 && static_cast<int>(ws.id) <= right_idx + 1);
        wlr_scene_node_set_enabled(&ws.scene_tree->node, is_near);

        double base_x = static_cast<double>(ws.id - 1) * vw;
        int32_t current_x = static_cast<int32_t>(std::round(base_x - cur_val));
        wlr_scene_node_set_position(&ws.scene_tree->node, current_x, 0);
    }

    if (m_frame_scheduler) {
        m_frame_scheduler();
    }
}

void WorkspaceManager::end_gesture_swipe(bool cancelled) noexcept {
    if (!m_in_gesture) return;

    m_in_gesture = false;
    double vw = static_cast<double>(m_viewport_width > 0 ? m_viewport_width : 1920);
    uint32_t target_ws = m_gesture_start_ws;
    double start_offset = static_cast<double>(m_gesture_start_ws - 1) * vw;
    double total_travel = m_slide_spring.value - start_offset; // positive = towards higher workspace

    if (!cancelled) {
        // 1. Flick velocity detection (macOS flick)
        constexpr double FLICK_VELOCITY_THRESHOLD = 500.0; // px/sec
        if (std::abs(m_gesture_velocity) > FLICK_VELOCITY_THRESHOLD) {
            if (m_gesture_velocity > FLICK_VELOCITY_THRESHOLD && m_gesture_start_ws < m_workspaces.size()) {
                target_ws = m_gesture_start_ws + 1;
            } else if (m_gesture_velocity < -FLICK_VELOCITY_THRESHOLD && m_gesture_start_ws > 1) {
                target_ws = m_gesture_start_ws - 1;
            }
        } else {
            // 2. Distance travel threshold (25% of viewport width)
            double dist_threshold = vw * 0.25;
            if (total_travel > dist_threshold && m_gesture_start_ws < m_workspaces.size()) {
                target_ws = m_gesture_start_ws + 1;
            } else if (total_travel < -dist_threshold && m_gesture_start_ws > 1) {
                target_ws = m_gesture_start_ws - 1;
            }
        }
    }

    // Hand-off to spring animation with release momentum
    m_active_id = target_ws;
    for (auto& ws : m_workspaces) {
        ws.is_active = (ws.id == target_ws);
    }

    double target_offset = static_cast<double>(target_ws - 1) * vw;
    m_slide_spring.target = target_offset;
    // Clamp inherited release velocity to prevent overshooting wildly
    m_slide_spring.velocity = std::clamp(m_gesture_velocity, -2500.0, 2500.0);
    m_is_sliding = true;

    if (m_frame_scheduler) {
        m_frame_scheduler();
    }

    log::info("[Gesture] WorkspaceManager: Swipe end (target_ws={}, velocity={:.1f}, travel={:.1f})",
              target_ws, m_gesture_velocity, total_travel);
}

void WorkspaceManager::set_workspace_scene_tree(uint32_t workspace_id, struct wlr_scene_tree* tree) noexcept {
    for (auto& ws : m_workspaces) {
        if (ws.id == workspace_id) {
            ws.scene_tree = tree;
            return;
        }
    }
}

struct wlr_scene_tree* WorkspaceManager::get_workspace_scene_tree(uint32_t workspace_id) const noexcept {
    for (const auto& ws : m_workspaces) {
        if (ws.id == workspace_id) return ws.scene_tree;
    }
    return nullptr;
}

struct wlr_scene_tree* WorkspaceManager::active_scene_tree() const noexcept {
    return get_workspace_scene_tree(m_active_id);
}

void WorkspaceManager::set_viewport_width(uint32_t width) noexcept {
    if (width == 0 || width == m_viewport_width) return;
    m_viewport_width = width;

    if (!m_is_sliding) {
        m_slide_spring.reset(static_cast<double>(m_active_id - 1) * static_cast<double>(m_viewport_width),
                             static_cast<double>(m_active_id - 1) * static_cast<double>(m_viewport_width));
        for (auto& ws : m_workspaces) {
            if (!ws.scene_tree) continue;
            double base_x = static_cast<double>(ws.id - 1) * static_cast<double>(m_viewport_width);
            int32_t cur_x = static_cast<int32_t>(std::round(base_x - m_slide_spring.value));
            wlr_scene_node_set_position(&ws.scene_tree->node, cur_x, 0);
        }
    }
}

void WorkspaceManager::add_window_to_workspace(uint32_t workspace_id, uint64_t window_id) {
    remove_window_from_workspace(window_id);

    auto it = std::find_if(m_workspaces.begin(), m_workspaces.end(),
                           [workspace_id](const Workspace& ws) { return ws.id == workspace_id; });

    if (it != m_workspaces.end()) {
        it->window_ids.push_back(window_id);
        m_window_to_workspace[window_id] = workspace_id;
    }
}

void WorkspaceManager::remove_window_from_workspace(uint64_t window_id) {
    auto map_it = m_window_to_workspace.find(window_id);
    if (map_it == m_window_to_workspace.end()) return;

    uint32_t ws_id = map_it->second;
    m_window_to_workspace.erase(map_it);

    auto ws_it = std::find_if(m_workspaces.begin(), m_workspaces.end(),
                              [ws_id](const Workspace& ws) { return ws.id == ws_id; });

    if (ws_it != m_workspaces.end()) {
        auto& win_vec = ws_it->window_ids;
        win_vec.erase(std::remove(win_vec.begin(), win_vec.end(), window_id), win_vec.end());
    }
}

const std::vector<Workspace>& WorkspaceManager::get_all_workspaces() const noexcept {
    return m_workspaces;
}

const Workspace* WorkspaceManager::get_workspace(uint32_t workspace_id) const noexcept {
    for (const auto& ws : m_workspaces) {
        if (ws.id == workspace_id) return &ws;
    }
    return nullptr;
}

Workspace* WorkspaceManager::get_workspace(uint32_t workspace_id) noexcept {
    for (auto& ws : m_workspaces) {
        if (ws.id == workspace_id) return &ws;
    }
    return nullptr;
}

bool WorkspaceManager::toggle_overview() {
    m_overview_active = !m_overview_active;
    log::info("WorkspaceManager: Mission Control (Overview Mode) toggled: {}", m_overview_active ? "ON" : "OFF");
    return true;
}

} // namespace tinexus::comp
