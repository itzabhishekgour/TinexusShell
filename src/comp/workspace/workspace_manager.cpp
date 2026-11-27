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
    m_window_to_workspace.clear();
    m_pending_destroy_ws_ids.clear();
    m_workspaces.reserve(count);
    m_next_fullscreen_id = 100;

    for (uint32_t i = 1; i <= count; ++i) {
        m_workspaces.push_back(Workspace{
            i,
            "Desktop " + std::to_string(i),
            (i == 1),
            WorkspaceKind::Desktop,
            0,
            i,
            false,
            {},
            nullptr
        });
    }
    m_active_id = 1;
    m_slide_spring.reset(0.0, 0.0);
    m_is_sliding = false;
    m_in_gesture = false;
    notify_state_changed();
}

uint32_t WorkspaceManager::active_workspace_id() const noexcept {
    return m_active_id;
}

int32_t WorkspaceManager::workspace_slot(uint32_t workspace_id) const noexcept {
    for (size_t i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces[i].id == workspace_id) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

uint32_t WorkspaceManager::active_slot_1based() const noexcept {
    int32_t slot = workspace_slot(m_active_id);
    return (slot >= 0) ? static_cast<uint32_t>(slot + 1) : 1u;
}

const Workspace* WorkspaceManager::active_workspace() const noexcept {
    return get_workspace(m_active_id);
}

bool WorkspaceManager::is_active_space_fullscreen() const noexcept {
    const Workspace* ws = active_workspace();
    return ws != nullptr && ws->kind == WorkspaceKind::Fullscreen;
}

uint32_t WorkspaceManager::fullscreen_space_count() const noexcept {
    uint32_t count = 0;
    for (const auto& ws : m_workspaces) {
        if (ws.kind == WorkspaceKind::Fullscreen &&
            m_pending_destroy_ws_ids.find(ws.id) == m_pending_destroy_ws_ids.end()) {
            ++count;
        }
    }
    return count;
}

uint32_t WorkspaceManager::active_or_populated_space_count() const noexcept {
    uint32_t count = 0;
    for (const auto& ws : m_workspaces) {
        if (m_pending_destroy_ws_ids.find(ws.id) != m_pending_destroy_ws_ids.end()) {
            continue;
        }
        if (ws.is_active || ws.kind == WorkspaceKind::Fullscreen || !ws.window_ids.empty()) {
            ++count;
        }
    }
    return std::max<uint32_t>(1, count);
}

void WorkspaceManager::notify_state_changed() noexcept {
    if (m_state_callback) {
        m_state_callback();
    }
}

void WorkspaceManager::reposition_all_workspace_trees() noexcept {
    const double vw = static_cast<double>(m_viewport_width);
    for (size_t i = 0; i < m_workspaces.size(); ++i) {
        auto& ws = m_workspaces[i];
        if (!ws.scene_tree) continue;
        double base_x = static_cast<double>(i) * vw;
        int32_t cur_x = static_cast<int32_t>(std::round(base_x - m_slide_spring.value));
        wlr_scene_node_set_position(&ws.scene_tree->node, cur_x, 0);
    }
}

void WorkspaceManager::prune_pending_fullscreen_spaces() noexcept {
    if (m_pending_destroy_ws_ids.empty()) return;

    bool erased_any = false;
    for (auto it = m_workspaces.begin(); it != m_workspaces.end(); ) {
        if (it->id != m_active_id &&
            m_pending_destroy_ws_ids.find(it->id) != m_pending_destroy_ws_ids.end()) {
            m_pending_destroy_ws_ids.erase(it->id);
            struct wlr_scene_tree* dead_tree = it->scene_tree;
            it->scene_tree = nullptr;
            it = m_workspaces.erase(it);
            if (dead_tree && m_tree_destroy_cb) {
                m_tree_destroy_cb(dead_tree);
            }
            erased_any = true;
        } else {
            ++it;
        }
    }

    if (erased_any) {
        int32_t active_slot = workspace_slot(m_active_id);
        if (active_slot < 0) active_slot = 0;
        double vw = static_cast<double>(m_viewport_width);
        m_slide_spring.reset(static_cast<double>(active_slot) * vw,
                             static_cast<double>(active_slot) * vw);
        reposition_all_workspace_trees();
        notify_state_changed();
    }
}

bool WorkspaceManager::switch_workspace(uint32_t workspace_id) {
    if (workspace_id == m_active_id && !m_is_sliding) return true;

    int32_t to_slot = workspace_slot(workspace_id);
    if (to_slot < 0) {
        log::warn("Attempted to switch to invalid workspace ID: {}", workspace_id);
        return false;
    }

    int32_t from_slot = workspace_slot(m_active_id);
    if (from_slot < 0) from_slot = 0;

    int32_t min_slot = std::min(from_slot, to_slot);
    int32_t max_slot = std::max(from_slot, to_slot);
    for (int32_t i = 0; i < static_cast<int32_t>(m_workspaces.size()); ++i) {
        auto& ws = m_workspaces[static_cast<size_t>(i)];
        if (ws.scene_tree && i >= min_slot && i <= max_slot) {
            wlr_scene_node_set_enabled(&ws.scene_tree->node, true);
        }
        ws.is_active = (ws.id == workspace_id);
    }

    m_active_id = workspace_id;

    if (!m_is_sliding) {
        m_slide_spring.velocity = 0.0;
    }
    double target_offset = static_cast<double>(to_slot) * static_cast<double>(m_viewport_width);
    m_slide_spring.target = target_offset;
    m_is_sliding = true;

    if (m_frame_scheduler) {
        m_frame_scheduler();
    }
    notify_state_changed();

    log::info("Switched to Workspace {} (slot={}, target_offset={})", workspace_id, to_slot + 1, target_offset);
    return true;
}

bool WorkspaceManager::switch_to_slot(uint32_t slot_1based) {
    if (slot_1based == 0 || slot_1based > m_workspaces.size()) {
        return false;
    }
    return switch_workspace(m_workspaces[slot_1based - 1].id);
}

uint32_t WorkspaceManager::find_fullscreen_space_for_window(uint64_t window_id) const noexcept {
    for (const auto& ws : m_workspaces) {
        if (ws.kind == WorkspaceKind::Fullscreen && ws.owner_window_id == window_id) {
            return ws.id;
        }
    }
    return 0;
}

uint32_t WorkspaceManager::promote_to_fullscreen_space(uint64_t window_id,
                                                       uint32_t from_workspace_id,
                                                       bool was_maximized,
                                                       const std::string& label,
                                                       struct wlr_scene_tree* fs_scene_tree) {
    uint32_t existing_fs = find_fullscreen_space_for_window(window_id);
    if (existing_fs != 0) {
        switch_workspace(existing_fs);
        return existing_fs;
    }

    int32_t cur_slot = workspace_slot(m_active_id);
    int32_t base_slot = workspace_slot(from_workspace_id);
    if (base_slot < 0) base_slot = (cur_slot >= 0) ? cur_slot : 0;

    // Insert right after base Desktop Space and any earlier consecutive Fullscreen Spaces attached to it
    size_t insert_idx = static_cast<size_t>(base_slot + 1);
    while (insert_idx < m_workspaces.size() &&
           m_workspaces[insert_idx].kind == WorkspaceKind::Fullscreen &&
           m_workspaces[insert_idx].restore_workspace_id == from_workspace_id) {
        ++insert_idx;
    }

    uint32_t new_id = m_next_fullscreen_id++;
    Workspace fs_ws;
    fs_ws.id = new_id;
    fs_ws.name = label.empty() ? "Fullscreen" : label;
    fs_ws.is_active = false;
    fs_ws.kind = WorkspaceKind::Fullscreen;
    fs_ws.owner_window_id = window_id;
    fs_ws.restore_workspace_id = from_workspace_id;
    fs_ws.was_maximized = was_maximized;
    fs_ws.scene_tree = fs_scene_tree;

    m_workspaces.insert(m_workspaces.begin() + static_cast<ptrdiff_t>(insert_idx), std::move(fs_ws));

    // Keep spring offset locked to the previous active slot if insertion shifted it right
    if (cur_slot >= 0 && insert_idx <= static_cast<size_t>(cur_slot)) {
        const double vw = static_cast<double>(m_viewport_width);
        m_slide_spring.value += vw;
        m_slide_spring.target += vw;
    }

    add_window_to_workspace(new_id, window_id);
    reposition_all_workspace_trees();

    if (fs_scene_tree) {
        wlr_scene_node_set_enabled(&fs_scene_tree->node, true);
    }

    switch_workspace(new_id);
    log::info("[Spaces] Promoted window {} to Dedicated Fullscreen Space {} ('{}', restore_ws={}, was_max={})",
              window_id, new_id, label, from_workspace_id, was_maximized);
    return new_id;
}

uint32_t WorkspaceManager::demote_fullscreen_space(uint64_t window_id, bool immediate_remove) {
    uint32_t fs_id = find_fullscreen_space_for_window(window_id);
    if (fs_id == 0) {
        return m_active_id;
    }

    uint32_t restore_ws_id = 1;
    for (auto& ws : m_workspaces) {
        if (ws.id == fs_id) {
            restore_ws_id = ws.restore_workspace_id;
            ws.owner_window_id = 0;
            break;
        }
    }
    if (get_workspace(restore_ws_id) == nullptr) {
        restore_ws_id = 1;
    }

    add_window_to_workspace(restore_ws_id, window_id);

    if (immediate_remove || m_active_id != fs_id) {
        int32_t rm_slot = workspace_slot(fs_id);
        int32_t cur_slot = workspace_slot(m_active_id);
        for (auto it = m_workspaces.begin(); it != m_workspaces.end(); ++it) {
            if (it->id == fs_id) {
                struct wlr_scene_tree* dead_tree = it->scene_tree;
                it->scene_tree = nullptr;
                m_workspaces.erase(it);
                if (dead_tree && m_tree_destroy_cb) {
                    m_tree_destroy_cb(dead_tree);
                }
                break;
            }
        }
        if (rm_slot >= 0 && cur_slot > rm_slot) {
            const double vw = static_cast<double>(m_viewport_width);
            m_slide_spring.value = std::max(0.0, m_slide_spring.value - vw);
            m_slide_spring.target = std::max(0.0, m_slide_spring.target - vw);
        }
        reposition_all_workspace_trees();
        if (m_active_id == fs_id) {
            m_active_id = restore_ws_id;
            for (auto& ws : m_workspaces) {
                ws.is_active = (ws.id == restore_ws_id);
                if (ws.scene_tree) {
                    wlr_scene_node_set_enabled(&ws.scene_tree->node, ws.is_active);
                }
            }
            const int32_t target_slot = std::max(0, workspace_slot(restore_ws_id));
            m_slide_spring.reset(static_cast<double>(target_slot) * static_cast<double>(m_viewport_width),
                                 static_cast<double>(target_slot) * static_cast<double>(m_viewport_width));
            reposition_all_workspace_trees();
        }
        notify_state_changed();
        return restore_ws_id;
    }

    // Slide smoothly back to restore_ws_id first, then prune fs_id when settled
    m_pending_destroy_ws_ids.insert(fs_id);
    switch_workspace(restore_ws_id);
    log::info("[Spaces] Scheduled demotion of Fullscreen Space {} back to Desktop {}", fs_id, restore_ws_id);
    return restore_ws_id;
}

void WorkspaceManager::tick_animation(double dt) noexcept {
    if (!m_is_sliding) return;

    // Critically damped zero-overshoot glide: stiffness=280.0, damping=34.2 (zeta ~ 1.022)
    // with pixel-appropriate settle thresholds (0.25 px, 2.5 px/s) for clean handoff.
    bool settled = m_slide_spring.step(dt, 280.0, 34.2, 0.25, 2.5);

    reposition_all_workspace_trees();

    if (settled) {
        m_is_sliding = false;
        // Disable off-screen workspaces to eliminate redundant damage & cursor checks
        for (auto& ws : m_workspaces) {
            if (ws.scene_tree && ws.id != m_active_id) {
                wlr_scene_node_set_enabled(&ws.scene_tree->node, false);
            }
        }
        prune_pending_fullscreen_spaces();
        notify_state_changed();
        log::info("Workspace slide settled on Workspace {}", m_active_id);
    }
}

void WorkspaceManager::begin_gesture_swipe() noexcept {
    if (m_in_gesture) return;

    m_in_gesture = true;
    m_is_sliding = false; // Halt any active spring animation

    double cur_val = m_slide_spring.value;
    double vw = effective_viewport_width();
    int max_slot = static_cast<int>(m_workspaces.size()) - 1;
    int nearest_slot = static_cast<int>(std::round(cur_val / vw));
    m_gesture_start_slot = static_cast<uint32_t>(std::clamp<int>(nearest_slot, 0, std::max(0, max_slot)));

    m_gesture_accum_dx = 0.0;
    m_gesture_last_time = 0;
    m_gesture_velocity = 0.0;

    int min_slot = (m_gesture_start_slot > 0) ? static_cast<int>(m_gesture_start_slot) - 1 : 0;
    int end_slot = std::min<int>(static_cast<int>(m_gesture_start_slot) + 1, max_slot);
    for (int i = 0; i < static_cast<int>(m_workspaces.size()); ++i) {
        auto& ws = m_workspaces[static_cast<size_t>(i)];
        if (ws.scene_tree && i >= min_slot && i <= end_slot) {
            wlr_scene_node_set_enabled(&ws.scene_tree->node, true);
        }
    }
    notify_state_changed();

    log::info("[Gesture] WorkspaceManager: Swipe begin (start_slot={}, current_offset={:.1f})",
              m_gesture_start_slot + 1, cur_val);
}

void WorkspaceManager::update_gesture_swipe(double dx, uint32_t time_msec) noexcept {
    if (!m_in_gesture) return;

    double vw = effective_viewport_width();

    if (m_gesture_last_time > 0 && time_msec > m_gesture_last_time) {
        double dt = static_cast<double>(time_msec - m_gesture_last_time) / 1000.0;
        if (dt > 0.001) {
            double instant_v = (-dx) / dt;
            m_gesture_velocity = (m_gesture_velocity * 0.3) + (instant_v * 0.7);
        }
    }
    m_gesture_last_time = time_msec;
    m_gesture_accum_dx += dx;

    double delta_offset = -dx;
    double max_offset = static_cast<double>(m_workspaces.size() - 1) * vw;

    if ((m_slide_spring.value < 0.0 && delta_offset < 0.0) ||
        (m_slide_spring.value > max_offset && delta_offset > 0.0)) {
        delta_offset *= 0.35;
    }

    m_slide_spring.value += delta_offset;

    double elastic_limit = vw * 0.25;
    m_slide_spring.value = std::clamp(m_slide_spring.value, -elastic_limit, max_offset + elastic_limit);

    double cur_val = m_slide_spring.value;
    int left_slot = static_cast<int>(std::floor(cur_val / vw));
    int right_slot = left_slot + 1;

    for (int i = 0; i < static_cast<int>(m_workspaces.size()); ++i) {
        auto& ws = m_workspaces[static_cast<size_t>(i)];
        if (!ws.scene_tree) continue;

        bool is_near = (i >= left_slot - 1 && i <= right_slot + 1);
        wlr_scene_node_set_enabled(&ws.scene_tree->node, is_near);

        double base_x = static_cast<double>(i) * vw;
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
    double vw = effective_viewport_width();
    uint32_t target_slot = m_gesture_start_slot;
    uint32_t max_slot = m_workspaces.empty() ? 0 : static_cast<uint32_t>(m_workspaces.size() - 1);
    double start_offset = static_cast<double>(m_gesture_start_slot) * vw;
    double total_travel = m_slide_spring.value - start_offset;

    if (!cancelled) {
        constexpr double FLICK_VELOCITY_THRESHOLD = 500.0;
        if (std::abs(m_gesture_velocity) > FLICK_VELOCITY_THRESHOLD) {
            if (m_gesture_velocity > FLICK_VELOCITY_THRESHOLD && m_gesture_start_slot < max_slot) {
                target_slot = m_gesture_start_slot + 1;
            } else if (m_gesture_velocity < -FLICK_VELOCITY_THRESHOLD && m_gesture_start_slot > 0) {
                target_slot = m_gesture_start_slot - 1;
            }
        } else {
            double dist_threshold = vw * 0.25;
            if (total_travel > dist_threshold && m_gesture_start_slot < max_slot) {
                target_slot = m_gesture_start_slot + 1;
            } else if (total_travel < -dist_threshold && m_gesture_start_slot > 0) {
                target_slot = m_gesture_start_slot - 1;
            }
        }
    }

    uint32_t target_ws = m_workspaces[target_slot].id;
    m_active_id = target_ws;
    for (auto& ws : m_workspaces) {
        ws.is_active = (ws.id == target_ws);
    }

    double target_offset = static_cast<double>(target_slot) * vw;
    m_slide_spring.target = target_offset;
    m_slide_spring.velocity = std::clamp(m_gesture_velocity, -1600.0, 1600.0);
    m_is_sliding = true;

    if (m_frame_scheduler) {
        m_frame_scheduler();
    }
    notify_state_changed();

    log::info("[Gesture] WorkspaceManager: Swipe end (target_ws={}, slot={}, velocity={:.1f}, travel={:.1f})",
              target_ws, target_slot + 1, m_gesture_velocity, total_travel);
}

void WorkspaceManager::set_workspace_scene_tree(uint32_t workspace_id, struct wlr_scene_tree* tree) noexcept {
    for (size_t i = 0; i < m_workspaces.size(); ++i) {
        auto& ws = m_workspaces[i];
        if (ws.id == workspace_id) {
            ws.scene_tree = tree;
            if (tree && m_viewport_width > 0) {
                int32_t initial_x = static_cast<int32_t>(i) * static_cast<int32_t>(m_viewport_width);
                wlr_scene_node_set_position(&tree->node, initial_x, 0);
            }
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

double WorkspaceManager::effective_viewport_width() const noexcept {
    if (m_viewport_width > 0) return static_cast<double>(m_viewport_width);
    const auto& outputs = OutputManager::instance().get_active_outputs();
    for (const auto& out : outputs) {
        if (out.width > 0) return static_cast<double>(out.width);
    }
    return 1920.0;
}

void WorkspaceManager::set_viewport_width(uint32_t width) noexcept {
    if (width == 0 || width == m_viewport_width) return;
    m_viewport_width = width;

    if (!m_is_sliding) {
        int32_t active_slot = std::max(0, workspace_slot(m_active_id));
        m_slide_spring.reset(static_cast<double>(active_slot) * static_cast<double>(m_viewport_width),
                             static_cast<double>(active_slot) * static_cast<double>(m_viewport_width));
        reposition_all_workspace_trees();
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
