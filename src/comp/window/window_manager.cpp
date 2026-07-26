#include "comp/window/window_manager.hpp"
#include "comp/window/focus_manager.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::comp {

WindowManager& WindowManager::instance() noexcept {
    static WindowManager s_instance;
    return s_instance;
}

FocusManager& FocusManager::instance() noexcept {
    static FocusManager s_instance;
    return s_instance;
}

uint64_t WindowManager::register_window(uint32_t pid, const std::string& app_id, const std::string& title) {
    uint64_t id = m_next_id++;
    uint32_t active_ws = WorkspaceManager::instance().active_workspace_id();

    WindowInfo info{id, pid, app_id, title, 100, 100, 800, 600, false, false, active_ws};
    m_windows.push_back(info);

    WorkspaceManager::instance().add_window_to_workspace(active_ws, id);
    log::info("Registered window ID: {}, app_id: '{}', title: '{}', workspace: {}", id, app_id, title, active_ws);

    FocusManager::instance().set_focus(id);
    return id;
}

void WindowManager::unregister_window(uint64_t window_id) {
    WorkspaceManager::instance().remove_window_from_workspace(window_id);

    auto it = std::find_if(m_windows.begin(), m_windows.end(),
                           [window_id](const WindowInfo& w) { return w.window_id == window_id; });

    if (it != m_windows.end()) {
        log::info("Unregistered window ID: {}, app_id: '{}'", window_id, it->app_id);
        m_windows.erase(it);
    }

    if (FocusManager::instance().focused_window_id() == window_id) {
        if (!m_windows.empty()) {
            FocusManager::instance().set_focus(m_windows.back().window_id);
        } else {
            FocusManager::instance().clear_focus();
        }
    }
}

void WindowManager::update_geometry(uint64_t window_id, int x, int y, int width, int height) {
    for (auto& w : m_windows) {
        if (w.window_id == window_id) {
            w.x = x;
            w.y = y;
            w.width = width;
            w.height = height;
            break;
        }
    }
}

void WindowManager::set_fullscreen(uint64_t window_id, bool fullscreen) {
    for (auto& w : m_windows) {
        if (w.window_id == window_id) {
            w.is_fullscreen = fullscreen;
            break;
        }
    }
}

std::optional<WindowInfo> WindowManager::get_window(uint64_t window_id) const {
    for (const auto& w : m_windows) {
        if (w.window_id == window_id) return w;
    }
    return std::nullopt;
}

std::vector<WindowInfo> WindowManager::get_all_windows() const {
    return m_windows;
}

std::optional<uint64_t> FocusManager::focused_window_id() const noexcept {
    return m_focused_id;
}

void FocusManager::set_focus(uint64_t window_id) {
    m_focused_id = window_id;
    for (auto& w : WindowManager::instance().get_all_windows()) {
        (void)w; // Focus state sync
    }
    log::info("Focused window ID: {}", window_id);
}

void FocusManager::clear_focus() {
    m_focused_id = std::nullopt;
    log::info("Cleared window focus");
}

} // namespace tinexus::comp
