#include "comp/workspace/workspace_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>

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

} // namespace tinexus::comp
