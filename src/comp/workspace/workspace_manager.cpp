#include "comp/workspace/workspace_manager.hpp"
#include "comp/window/window_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <cmath>

namespace tinexus::comp {

WorkspaceManager& WorkspaceManager::instance() noexcept {
    static WorkspaceManager s_instance;
    return s_instance;
}

WorkspaceManager::WorkspaceManager() {
    initialize_default_workspaces(3);
}

void WorkspaceManager::initialize_default_workspaces(uint32_t count) {
    m_workspaces.clear();
    m_workspaces.reserve(count);

    for (uint32_t i = 1; i <= count; ++i) {
        m_workspaces.push_back(Workspace{i, "Workspace " + std::to_string(i), (i == 1), {}});
    }
    m_active_id = 1;
}

uint32_t WorkspaceManager::active_workspace_id() const noexcept {
    return m_active_id;
}

bool WorkspaceManager::switch_workspace(uint32_t workspace_id) {
    if (workspace_id == m_active_id) return true;

    auto it = std::find_if(m_workspaces.begin(), m_workspaces.end(),
                           [workspace_id](const Workspace& ws) { return ws.id == workspace_id; });

    if (it == m_workspaces.end()) {
        log::warn("Attempted to switch to invalid workspace ID: {}", workspace_id);
        return false;
    }

    for (auto& ws : m_workspaces) {
        ws.is_active = (ws.id == workspace_id);
    }

    m_active_id = workspace_id;
    log::info("Switched to Workspace {}", workspace_id);
    return true;
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

bool WorkspaceManager::toggle_overview() {
    m_overview_active = !m_overview_active;
    log::info("WorkspaceManager: Mission Control (Overview Mode) toggled: {}", m_overview_active ? "ON" : "OFF");

    auto& win_mgr = WindowManager::instance();

    if (m_overview_active) {
        // Save current geometries of windows in the active workspace and arrange in a grid
        m_saved_geometries.clear();
        
        const Workspace* active_ws = get_workspace(m_active_id);
        if (!active_ws || active_ws->window_ids.empty()) return true;

        const auto& win_ids = active_ws->window_ids;
        size_t count = win_ids.size();

        // Save original geometry first
        for (uint64_t win_id : win_ids) {
            auto info_opt = win_mgr.get_window(win_id);
            if (info_opt) {
                m_saved_geometries[win_id] = SavedGeometry{
                    info_opt->x, info_opt->y, info_opt->width, info_opt->height
                };
            }
        }

        // Layout grid math: screen resolution is 1920x1080
        int32_t screen_w = 1920;
        int32_t screen_h = 1080;
        int32_t pad_x = 100;
        int32_t pad_y = 100;
        int32_t usable_w = screen_w - 2 * pad_x;
        int32_t usable_h = screen_h - 2 * pad_y;

        size_t cols = 1;
        size_t rows = 1;
        if (count == 2) {
            cols = 2; rows = 1;
        } else if (count <= 4) {
            cols = 2; rows = 2;
        } else {
            cols = 3; rows = static_cast<size_t>(std::ceil(static_cast<double>(count) / 3.0));
        }

        int32_t cell_w = usable_w / static_cast<int32_t>(cols);
        int32_t cell_h = usable_h / static_cast<int32_t>(rows);

        // Gap/Margin between windows: 20px
        int32_t gap = 20;

        for (size_t i = 0; i < count; ++i) {
            uint64_t win_id = win_ids[i];
            size_t r = i / cols;
            size_t c = i % cols;

            int32_t x = pad_x + static_cast<int32_t>(c) * cell_w + gap;
            int32_t y = pad_y + static_cast<int32_t>(r) * cell_h + gap;
            int32_t w = cell_w - 2 * gap;
            int32_t h = cell_h - 2 * gap;

            win_mgr.set_geometry(win_id, x, y, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            log::info("WorkspaceManager: Mission Control positioned Window #{} at ({},{}) size {}x{}",
                      win_id, x, y, w, h);
        }
    } else {
        // Restore all geometries
        for (const auto& [win_id, geom] : m_saved_geometries) {
            win_mgr.set_geometry(win_id, geom.x, geom.y, geom.width, geom.height);
            log::info("WorkspaceManager: Mission Control restored Window #{} to ({},{}) size {}x{}",
                      win_id, geom.x, geom.y, geom.width, geom.height);
        }
        m_saved_geometries.clear();
    }

    return true;
}

} // namespace tinexus::comp
