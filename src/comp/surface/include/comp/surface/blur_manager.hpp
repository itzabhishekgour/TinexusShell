// ============================================================================
// blur_manager.hpp — Wayland Blur Protocol Manager for Tinexus Compositor
// Ref: org-kde-kwin-blur.xml & docs/05_UI_UX_GUIDELINES.md §6.1
// ============================================================================
#ifndef TINEXUS_COMP_BLUR_MANAGER_HPP
#define TINEXUS_COMP_BLUR_MANAGER_HPP

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>
#include <pixman.h>

extern "C" {
#include <wayland-server-core.h>
}

struct wlr_surface;
struct wl_display;
struct wl_resource;
struct wl_listener;

namespace tinexus::comp {

struct BlurSurfaceState {
    struct wlr_surface* surface{nullptr};
    pixman_region32_t region;
    bool has_custom_region{false};
    int radius{28};
    uint32_t tint{0x13131ACC}; // Standard Tinexus translucent fluid tint
    struct wl_listener destroy_listener;
};

class BlurManager {
public:
    static BlurManager& instance();

    bool initialize(struct wl_display* display);
    void shutdown();

    // Protocol handlers
    void register_surface_blur(struct wlr_surface* surface, struct wl_resource* blur_resource);
    void set_surface_blur_region(struct wlr_surface* surface, struct wl_resource* region_resource);
    void commit_surface_blur(struct wlr_surface* surface);
    void unregister_surface_blur(struct wlr_surface* surface);

    // Queries
    bool is_surface_blurred(struct wlr_surface* surface) const;
    const BlurSurfaceState* get_surface_state(struct wlr_surface* surface) const;

    // Direct namespace/app query for built-in shell elements
    bool is_namespace_blurred(const std::string& ns) const;

private:
    BlurManager() = default;
    ~BlurManager();

    BlurManager(const BlurManager&) = delete;
    BlurManager& operator=(const BlurManager&) = delete;

    struct wl_global* m_global{nullptr};
    struct wl_display* m_display{nullptr};

    std::unordered_map<struct wlr_surface*, std::unique_ptr<BlurSurfaceState>> m_surfaces;
    std::unordered_set<std::string> m_blurred_namespaces{
        "topbar", "tinexus-shell", "launcher", "tinexus-launcher", "dock", "tinexus-dock", "notifications"
    };
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_BLUR_MANAGER_HPP
