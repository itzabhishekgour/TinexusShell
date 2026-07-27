#include "comp/server/surface_tree.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

WindowID SurfaceTree::create_surface(const std::string& app_id, const std::string& title) {
    WindowID id = ++m_next_id;
    auto node = std::make_shared<SurfaceNode>();
    node->window_id = id;
    node->app_id = app_id;
    node->title = title;
    m_roots.push_back(node);

    log::info("SurfaceTree: Created surface node WindowID #{} for app '{}' ({})", id, app_id, title);
    return id;
}

bool SurfaceTree::destroy_surface(WindowID id) {
    for (auto it = m_roots.begin(); it != m_roots.end(); ++it) {
        if ((*it)->window_id == id) {
            log::info("SurfaceTree: Destroyed surface node WindowID #{}", id);
            m_roots.erase(it);
            return true;
        }
    }
    return false;
}

size_t SurfaceTree::active_surface_count() const noexcept {
    return m_roots.size();
}

} // namespace tinexus::comp
