#include "comp/window/window_manager.hpp"
#include "comp/focus/focus_manager.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::comp {

WindowManager& WindowManager::instance() noexcept {
    static WindowManager s_instance;
    return s_instance;
}

uint64_t WindowManager::register_window(uint32_t pid, const std::string& app_id, const std::string& title) {
    uint64_t id = m_next_id++;
    uint32_t active_ws = WorkspaceManager::instance().active_workspace_id();

    WindowInfo info{id, pid, app_id, title, 100, 100, 800, 600, false, false, false, active_ws};
    m_windows.push_back(info);

    WorkspaceManager::instance().add_window_to_workspace(active_ws, id);
    log::info("Registered window ID: {}, app_id: '{}', title: '{}', workspace: {}", id, app_id, title, active_ws);

    FocusManager::instance().set_focus(FocusTargetType::Window, id, app_id);
    return id;
}

bool WindowManager::unregister_window(uint64_t window_id) {
    auto it = std::find_if(m_windows.begin(), m_windows.end(), [window_id](const WindowInfo& w) {
        return w.window_id == window_id;
    });

    if (it != m_windows.end()) {
        WorkspaceManager::instance().remove_window_from_workspace(window_id);
        log::info("Unregistered window ID: {}", window_id);
        m_windows.erase(it);
        return true;
    }

    return false;
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

void WindowManager::set_geometry(uint64_t window_id, int x, int y, int width, int height) {
    for (auto& w : m_windows) {
        if (w.window_id == window_id) {
            w.x = x;
            w.y = y;
            w.width = width;
            w.height = height;
            log::debug("Window ID: {} geometry updated: {}x{} @ ({},{})", window_id, width, height, x, y);
            break;
        }
    }
}

void WindowManager::set_fullscreen(uint64_t window_id, bool fullscreen) {
    for (auto& w : m_windows) {
        if (w.window_id == window_id) {
            w.is_fullscreen = fullscreen;
            log::info("Window ID: {} fullscreen set to {}", window_id, fullscreen);
            break;
        }
    }
}

void WindowManager::set_minimized(uint64_t window_id, bool minimized) {
    for (auto& w : m_windows) {
        if (w.window_id == window_id) {
            w.is_minimized = minimized;
            log::info("Window ID: {} minimized set to {}", window_id, minimized);
            break;
        }
    }
}

} // namespace tinexus::comp
