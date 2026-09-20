#ifndef TINEXUS_COMP_DECORATION_MANAGER_HPP
#define TINEXUS_COMP_DECORATION_MANAGER_HPP

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>

extern "C" {
#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
struct wlr_xwayland_surface;
#define static
#include <wlr/types/wlr_scene.h>
#undef static
}

namespace tinexus::comp {

enum class HitTarget : uint8_t {
    None,
    TitlebarDrag,
    CloseButton,
    MinimizeButton,
    MaximizeButton,
    BorderTop,
    BorderBottom,
    BorderLeft,
    BorderRight
};

struct IWindowActionHandler {
    virtual ~IWindowActionHandler() = default;
    virtual void request_move(void* wrapper) = 0;
    virtual void request_resize(void* wrapper, uint32_t edges) = 0;
    virtual void request_maximize(void* wrapper, bool maximize) = 0;
    virtual void request_minimize(void* wrapper, bool minimize) = 0;
    virtual void request_close(void* wrapper) = 0;
    [[nodiscard]] virtual bool is_window_maximized(void* wrapper) const = 0;
    virtual void on_decoration_mode_changed(struct wlr_xdg_toplevel* /*toplevel*/) {}
};

struct TinexusWindowFrame {
    struct wlr_scene_tree* frame_tree{nullptr}; // Root node under m_scene_tree_normal
    struct wlr_scene_tree* client_tree{nullptr}; // Child node holding the client surface at (1, 29)
    struct wlr_scene_rect* titlebar_bg{nullptr};
    struct wlr_scene_rect* titlebar_divider{nullptr};
    struct wlr_scene_rect* border_top{nullptr};
    struct wlr_scene_rect* border_bottom{nullptr};
    struct wlr_scene_rect* border_left{nullptr};
    struct wlr_scene_rect* border_right{nullptr};

    // Traffic light buttons (circular multi-slice scene rects)
    struct wlr_scene_tree* traffic_tree{nullptr};
    std::vector<struct wlr_scene_rect*> close_rects;
    std::vector<struct wlr_scene_rect*> min_rects;
    std::vector<struct wlr_scene_rect*> max_rects;

    int32_t client_width{800};
    int32_t client_height{600};
    bool is_active{false};
    bool is_fullscreen{false};

    void update_geometry(int32_t w, int32_t h);
    void set_active(bool active);
    void set_fullscreen(bool fullscreen);
    [[nodiscard]] HitTarget hit_test(double local_x, double local_y) const;
};

class TinexusDecorationManager {
public:
    static TinexusDecorationManager& instance() noexcept;

    bool init(struct wl_display* display, IWindowActionHandler* action_handler, struct wlr_scene_tree* scene_tree_normal);
    void shutdown();

    // Policy check: true for native Tinexus apps or CSD-preferring external apps (Firefox, Chromium)
    static bool is_native_csd_app(const char* app_id) noexcept;

    // Decoration negotiation queries
    [[nodiscard]] bool client_wants_csd(struct wlr_xdg_toplevel* toplevel) const;
    [[nodiscard]] bool client_wants_ssd(struct wlr_xdg_toplevel* toplevel) const;

    // Frame lifecycle (Native Wayland toplevels)
    TinexusWindowFrame* create_frame(struct wlr_xdg_toplevel* toplevel, struct wlr_scene_tree* parent = nullptr);
    void destroy_frame(struct wlr_xdg_toplevel* toplevel);
    [[nodiscard]] TinexusWindowFrame* get_frame(struct wlr_xdg_toplevel* toplevel) const;

    // Frame lifecycle (XWayland surfaces)
    TinexusWindowFrame* create_xwayland_frame(struct wlr_xwayland_surface* xsurface, struct wlr_scene_tree* parent = nullptr);
    void destroy_xwayland_frame(struct wlr_xwayland_surface* xsurface);
    [[nodiscard]] TinexusWindowFrame* get_xwayland_frame(struct wlr_xwayland_surface* xsurface) const;
    void set_xwayland_active(struct wlr_xwayland_surface* xsurface, bool active);
    void update_xwayland_geometry(struct wlr_xwayland_surface* xsurface, int32_t w, int32_t h);

    // Handle cursor button on potential frame
    bool handle_cursor_button(struct wlr_scene_node* node, double cursor_x, double cursor_y,
                              uint32_t button, uint32_t state, void* wrapper);

    // Focus state updates
    void set_toplevel_active(struct wlr_xdg_toplevel* toplevel, bool active);

    // Fullscreen state updates
    void set_toplevel_fullscreen(struct wlr_xdg_toplevel* toplevel, bool fullscreen);

    // Geometry updates from commit
    void update_toplevel_geometry(struct wlr_xdg_toplevel* toplevel, int32_t w, int32_t h);

private:
    TinexusDecorationManager() = default;
    ~TinexusDecorationManager() = default;

    TinexusWindowFrame* create_frame_impl(void* window_key, struct wlr_scene_tree* parent, const char* label);
    void destroy_frame_impl(void* window_key);
    [[nodiscard]] TinexusWindowFrame* get_frame_impl(void* window_key) const;

    static void handle_new_toplevel_decoration(struct wl_listener* listener, void* data);
    static void handle_decoration_request_mode(struct wl_listener* listener, void* data);
    static void handle_decoration_destroy(struct wl_listener* listener, void* data);

    struct DecorationContext {
        struct wlr_xdg_toplevel_decoration_v1* decoration{nullptr};
        struct wl_listener request_mode{};
        struct wl_listener destroy{};
        TinexusDecorationManager* manager{nullptr};
    };

    struct wl_display* m_display{nullptr};
    IWindowActionHandler* m_action_handler{nullptr};
    struct wlr_scene_tree* m_scene_tree_normal{nullptr};
    struct wlr_xdg_decoration_manager_v1* m_manager_v1{nullptr};
    struct wl_listener m_new_decoration_listener{};

    std::unordered_map<void*, std::unique_ptr<TinexusWindowFrame>> m_frames;
    std::vector<std::unique_ptr<DecorationContext>> m_contexts;

    // Double-click detection on titlebar
    uint32_t m_last_click_time{0};
    void* m_last_click_wrapper{nullptr};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_DECORATION_MANAGER_HPP
