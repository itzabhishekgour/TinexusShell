#include "comp/window/window_manager.hpp"
#include "comp/focus/focus_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

WindowManager& WindowManager::instance() {
    static WindowManager s_instance;
    return s_instance;
}

std::shared_ptr<WindowNode> WindowManager::create_window(uint64_t surface_id, const std::string& app_id) {
    auto win = std::make_shared<WindowNode>(surface_id, app_id);
    m_windows[surface_id] = win;
    log::info("WindowManager: Created managed window for Surface #{} ({})", surface_id, app_id);
    return win;
}

bool WindowManager::map_window(uint64_t surface_id, SceneGraph& scene_graph) {
    auto win = find_window(surface_id);
    if (!win) return false;

    win->visible = true;
    scene_graph.add_node(win);
    log::info("WindowManager: Mapped window #{} into SceneGraph", surface_id);
    return true;
}

bool WindowManager::unmap_window(uint64_t surface_id, SceneGraph& scene_graph) {
    auto win = find_window(surface_id);
    if (!win) return false;

    win->visible = false;
    scene_graph.remove_node(surface_id);
    m_windows.erase(surface_id);
    log::info("WindowManager: Unmapped and removed window #{} from SceneGraph", surface_id);
    return true;
}

std::shared_ptr<WindowNode> WindowManager::find_window(uint64_t surface_id) const {
    auto it = m_windows.find(surface_id);
    return (it != m_windows.end()) ? it->second : nullptr;
}

uint64_t WindowManager::register_window(uint64_t surface_id, const std::string& app_id, const std::string& title) {
    auto win = create_window(surface_id, app_id);
    win->toplevel.set_title(title);
    FocusManager::instance().set_focus(FocusTargetType::Window, surface_id, app_id);
    return surface_id;
}

void WindowManager::unregister_window(uint64_t surface_id) {
    m_windows.erase(surface_id);
}

void WindowManager::set_geometry(uint64_t surface_id, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    auto win = find_window(surface_id);
    if (win) {
        win->x = x;
        win->y = y;
        win->width = static_cast<int32_t>(width);
        win->height = static_cast<int32_t>(height);
    }
}

std::optional<WindowInfo> WindowManager::get_window(uint64_t surface_id) const {
    auto win = find_window(surface_id);
    if (!win) return std::nullopt;

    WindowInfo info;
    info.surface_id = surface_id;
    info.app_id = win->toplevel.app_id();
    info.title = win->toplevel.title();
    info.x = win->x;
    info.y = win->y;
    info.width = static_cast<uint32_t>(win->width);
    info.height = static_cast<uint32_t>(win->height);
    return info;
}

} // namespace tinexus::comp
