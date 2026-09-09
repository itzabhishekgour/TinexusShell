#ifndef TINEXUS_COMP_WINDOW_MANAGER_HPP
#define TINEXUS_COMP_WINDOW_MANAGER_HPP

#include "comp/window/scene_graph.hpp"
#include "comp/window/window_node.hpp"
#include <unordered_map>
#include <memory>
#include <optional>

namespace tinexus::comp {

struct WindowInfo {
    uint64_t surface_id{0};
    std::string app_id;
    std::string title;
    int32_t x{0};
    int32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
};

class WindowManager {
public:
    WindowManager() = default;
    ~WindowManager() = default;

    static WindowManager& instance();

    std::shared_ptr<WindowNode> create_window(uint64_t surface_id, const std::string& app_id);
    bool map_window(uint64_t surface_id, SceneGraph& scene_graph);
    bool unmap_window(uint64_t surface_id, SceneGraph& scene_graph);

    [[nodiscard]] std::shared_ptr<WindowNode> find_window(uint64_t surface_id) const;
    [[nodiscard]] size_t managed_windows_count() const noexcept { return m_windows.size(); }

    void tick_animations(double dt);
    [[nodiscard]] bool has_active_animations() const noexcept;

    // Legacy unit test support
    uint64_t register_window(uint64_t surface_id, const std::string& app_id, const std::string& title);
    void unregister_window(uint64_t surface_id);
    void set_geometry(uint64_t surface_id, int32_t x, int32_t y, uint32_t width, uint32_t height);
    std::optional<WindowInfo> get_window(uint64_t surface_id) const;

private:
    std::unordered_map<uint64_t, std::shared_ptr<WindowNode>> m_windows;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WINDOW_MANAGER_HPP
