#include "comp/window/decoration_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <chrono>

extern "C" {
#include <wlr/util/edges.h>
#define class c_class
#include <wlr/xwayland/xwayland.h>
#undef class
}

namespace tinexus::comp {

namespace {

// Tinexus macOS Obsidian Palette tokens
constexpr float kTitleBgActive[4]    = {0.070f, 0.078f, 0.105f, 1.0f}; // #12141b
constexpr float kTitleBgInactive[4]  = {0.051f, 0.055f, 0.078f, 1.0f}; // #0d0e14
constexpr float kDividerActive[4]    = {0.125f, 0.137f, 0.180f, 1.0f}; // #20232e
constexpr float kDividerInactive[4]  = {0.090f, 0.094f, 0.133f, 1.0f}; // #171822
constexpr float kBorderActive[4]     = {0.110f, 0.120f, 0.160f, 1.0f}; // #1c1f29
constexpr float kBorderInactive[4]   = {0.078f, 0.082f, 0.110f, 1.0f}; // #14151c

// Colors matching MacTrafficLights.qml exactly:
// Close: fill #ff5f56, border #e0443e
constexpr float kCloseFillActive[4]    = {1.0f, 0.373f, 0.337f, 1.0f};
constexpr float kCloseBorderActive[4]  = {0.878f, 0.267f, 0.243f, 1.0f};
// Min: fill #ffbd2e, border #dea123
constexpr float kMinFillActive[4]      = {1.0f, 0.741f, 0.180f, 1.0f};
constexpr float kMinBorderActive[4]    = {0.871f, 0.631f, 0.137f, 1.0f};
// Max: fill #27c93f, border #1aab29
constexpr float kMaxFillActive[4]      = {0.153f, 0.788f, 0.247f, 1.0f};
constexpr float kMaxBorderActive[4]    = {0.102f, 0.671f, 0.161f, 1.0f};

// Inactive
constexpr float kInactiveFill[4]       = {0.25f, 0.27f, 0.32f, 1.0f};
constexpr float kInactiveBorder[4]     = {0.20f, 0.22f, 0.26f, 1.0f};

static void build_circle(struct wlr_scene_tree* parent, int pos_x, int pos_y,
                         const float fill[4], const float border[4],
                         std::vector<struct wlr_scene_rect*>& rect_list) {
    struct wlr_scene_tree* tree = wlr_scene_tree_create(parent);
    wlr_scene_node_set_position(&tree->node, pos_x, pos_y);

    auto add_border = [&](int x, int y, int w, int h) {
        auto* r = wlr_scene_rect_create(tree, w, h, border);
        wlr_scene_node_set_position(&r->node, x, y);
        rect_list.push_back(r);
    };
    auto add_fill = [&](int x, int y, int w, int h) {
        auto* r = wlr_scene_rect_create(tree, w, h, fill);
        wlr_scene_node_set_position(&r->node, x, y);
        rect_list.push_back(r);
    };

    // 1. Outer border mask slices (12x12 round circle)
    add_border(4, 0, 4, 1);
    add_border(2, 1, 8, 1);
    add_border(1, 2, 10, 1);
    add_border(0, 3, 12, 6);
    add_border(1, 9, 10, 1);
    add_border(2, 10, 8, 1);
    add_border(4, 11, 4, 1);

    // 2. Inner fill slices
    add_fill(4, 1, 4, 1);
    add_fill(2, 2, 8, 1);
    add_fill(1, 3, 10, 6);
    add_fill(2, 9, 8, 1);
    add_fill(4, 10, 4, 1);
}

} // namespace

// ---------------------------------------------------------------------------
// TinexusWindowFrame Member Functions
// ---------------------------------------------------------------------------

void TinexusWindowFrame::update_geometry(int32_t w, int32_t h) {
    client_width = std::max(100, w);
    client_height = std::max(100, h);

    int32_t total_w = client_width + 2;
    int32_t total_h = client_height + 28 + 2;

    if (titlebar_bg) {
        wlr_scene_rect_set_size(titlebar_bg, total_w, 28);
    }
    if (titlebar_divider) {
        wlr_scene_rect_set_size(titlebar_divider, total_w, 1);
    }
    if (border_top) {
        wlr_scene_rect_set_size(border_top, total_w, 1);
    }
    if (border_bottom) {
        wlr_scene_rect_set_size(border_bottom, total_w, 1);
        wlr_scene_node_set_position(&border_bottom->node, 0, total_h - 1);
    }
    if (border_left) {
        wlr_scene_rect_set_size(border_left, 1, total_h);
    }
    if (border_right) {
        wlr_scene_rect_set_size(border_right, 1, total_h);
        wlr_scene_node_set_position(&border_right->node, total_w - 1, 0);
    }
}

void TinexusWindowFrame::set_active(bool active) {
    is_active = active;

    if (titlebar_bg) {
        wlr_scene_rect_set_color(titlebar_bg, active ? kTitleBgActive : kTitleBgInactive);
    }
    if (titlebar_divider) {
        wlr_scene_rect_set_color(titlebar_divider, active ? kDividerActive : kDividerInactive);
    }
    if (border_top) {
        wlr_scene_rect_set_color(border_top, active ? kBorderActive : kBorderInactive);
    }
    if (border_bottom) {
        wlr_scene_rect_set_color(border_bottom, active ? kBorderActive : kBorderInactive);
    }
    if (border_left) {
        wlr_scene_rect_set_color(border_left, active ? kBorderActive : kBorderInactive);
    }
    if (border_right) {
        wlr_scene_rect_set_color(border_right, active ? kBorderActive : kBorderInactive);
    }

    for (size_t i = 0; i < close_rects.size(); ++i) {
        const float* col = (i < 7) ? (active ? kCloseBorderActive : kInactiveBorder)
                                   : (active ? kCloseFillActive : kInactiveFill);
        wlr_scene_rect_set_color(close_rects[i], col);
    }
    for (size_t i = 0; i < min_rects.size(); ++i) {
        const float* col = (i < 7) ? (active ? kMinBorderActive : kInactiveBorder)
                                   : (active ? kMinFillActive : kInactiveFill);
        wlr_scene_rect_set_color(min_rects[i], col);
    }
    for (size_t i = 0; i < max_rects.size(); ++i) {
        const float* col = (i < 7) ? (active ? kMaxBorderActive : kInactiveBorder)
                                   : (active ? kMaxFillActive : kInactiveFill);
        wlr_scene_rect_set_color(max_rects[i], col);
    }
}

void TinexusWindowFrame::set_fullscreen(bool fullscreen) {
    is_fullscreen = fullscreen;

    bool enabled = !fullscreen;
    if (titlebar_bg) wlr_scene_node_set_enabled(&titlebar_bg->node, enabled);
    if (titlebar_divider) wlr_scene_node_set_enabled(&titlebar_divider->node, enabled);
    if (traffic_tree) wlr_scene_node_set_enabled(&traffic_tree->node, enabled);
    if (border_top) wlr_scene_node_set_enabled(&border_top->node, enabled);
    if (border_bottom) wlr_scene_node_set_enabled(&border_bottom->node, enabled);
    if (border_left) wlr_scene_node_set_enabled(&border_left->node, enabled);
    if (border_right) wlr_scene_node_set_enabled(&border_right->node, enabled);

    if (client_tree) {
        if (fullscreen) {
            wlr_scene_node_set_position(&client_tree->node, 0, 0);
        } else {
            wlr_scene_node_set_position(&client_tree->node, 1, 29);
        }
    }
}

HitTarget TinexusWindowFrame::hit_test(double local_x, double local_y) const {
    if (is_fullscreen) {
        return HitTarget::None;
    }

    int32_t total_w = client_width + 2;
    int32_t total_h = client_height + 28 + 2;

    if (local_x < 0.0 || local_x >= total_w || local_y < 0.0 || local_y >= total_h) {
        return HitTarget::None;
    }

    // Outer border resize zones (3px tolerance)
    if (local_y <= 2.0) return HitTarget::BorderTop;
    if (local_y >= total_h - 3.0) return HitTarget::BorderBottom;
    if (local_x <= 2.0) return HitTarget::BorderLeft;
    if (local_x >= total_w - 3.0) return HitTarget::BorderRight;

    // Titlebar zone (y in [1..28])
    if (local_y >= 1.0 && local_y <= 28.0) {
        // Traffic lights click targets (matching 14px left margin and 20px circle centers)
        if (local_y >= 4.0 && local_y <= 24.0) {
            if (local_x >= 10.0 && local_x <= 28.0) return HitTarget::CloseButton;
            if (local_x >= 28.0 && local_x <= 48.0) return HitTarget::MinimizeButton;
            if (local_x >= 48.0 && local_x <= 68.0) return HitTarget::MaximizeButton;
        }
        return HitTarget::TitlebarDrag;
    }

    return HitTarget::None;
}

// ---------------------------------------------------------------------------
// TinexusDecorationManager Implementation
// ---------------------------------------------------------------------------

TinexusDecorationManager& TinexusDecorationManager::instance() noexcept {
    static TinexusDecorationManager s_instance;
    return s_instance;
}

bool TinexusDecorationManager::init(struct wl_display* display, IWindowActionHandler* action_handler,
                                   struct wlr_scene_tree* scene_tree_normal) {
    if (!display || !action_handler || !scene_tree_normal) {
        log::error("[Decoration] Invalid arguments provided to TinexusDecorationManager::init");
        return false;
    }

    m_display = display;
    m_action_handler = action_handler;
    m_scene_tree_normal = scene_tree_normal;

    m_manager_v1 = wlr_xdg_decoration_manager_v1_create(m_display);
    if (!m_manager_v1) {
        log::error("[Decoration] Failed to create wlr_xdg_decoration_manager_v1 global");
        return false;
    }

    m_new_decoration_listener.notify = handle_new_toplevel_decoration;
    wl_signal_add(&m_manager_v1->events.new_toplevel_decoration, &m_new_decoration_listener);

    log::info("[Decoration] TinexusDecorationManager initialized successfully with zxdg_decoration_manager_v1");
    return true;
}

void TinexusDecorationManager::shutdown() {
    if (m_new_decoration_listener.link.next) {
        wl_list_remove(&m_new_decoration_listener.link);
        m_new_decoration_listener.link.next = nullptr;
    }
    m_frames.clear();
    m_contexts.clear();
    m_manager_v1 = nullptr;
    m_display = nullptr;
    m_action_handler = nullptr;
    m_scene_tree_normal = nullptr;
    log::info("[Decoration] TinexusDecorationManager shut down cleanly");
}

bool TinexusDecorationManager::is_native_csd_app(const char* app_id) noexcept {
    if (!app_id || *app_id == '\0') {
        return false;
    }
    std::string_view id(app_id);

    // 1. Native Tinexus shell and UI components (Qt6 / txui client-side decoration)
    if (id.starts_with("tinexus-") ||
        id.starts_with("io.tinexus.") ||
        id.starts_with("txui-") ||
        id == "lock" ||
        id == "launcher") {
        return true;
    }

    // 2. Known external apps that reliably self-decorate (CSD) under Wayland
    if (id == "firefox" ||
        id == "org.mozilla.firefox" ||
        id == "chromium" ||
        id == "chromium-browser" ||
        id == "google-chrome") {
        return true;
    }

    return false;
}

bool TinexusDecorationManager::client_wants_csd(struct wlr_xdg_toplevel* toplevel) const {
    if (!toplevel) return false;
    for (const auto& ctx : m_contexts) {
        if (ctx && ctx->decoration && ctx->decoration->toplevel == toplevel) {
            return ctx->decoration->current.mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE ||
                   ctx->decoration->scheduled_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE ||
                   ctx->decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
        }
    }
    return false;
}

bool TinexusDecorationManager::client_wants_ssd(struct wlr_xdg_toplevel* toplevel) const {
    if (!toplevel) return false;
    for (const auto& ctx : m_contexts) {
        if (ctx && ctx->decoration && ctx->decoration->toplevel == toplevel) {
            return ctx->decoration->current.mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE ||
                   ctx->decoration->scheduled_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE ||
                   ctx->decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        }
    }
    return false;
}

TinexusWindowFrame* TinexusDecorationManager::create_frame_impl(void* window_key, struct wlr_scene_tree* parent, const char* label) {
    struct wlr_scene_tree* root_tree = parent ? parent : m_scene_tree_normal;
    if (!window_key || !root_tree) return nullptr;

    auto frame = std::make_unique<TinexusWindowFrame>();

    // 1. Root frame tree under parent workspace scene tree (or normal scene tree)
    frame->frame_tree = wlr_scene_tree_create(root_tree);
    if (!frame->frame_tree) return nullptr;

    // 2. Titlebar background (28px)
    frame->titlebar_bg = wlr_scene_rect_create(frame->frame_tree, 802, 28, kTitleBgActive);
    wlr_scene_node_set_position(&frame->titlebar_bg->node, 0, 1);

    // 3. Titlebar bottom divider (1px line at y=28)
    frame->titlebar_divider = wlr_scene_rect_create(frame->frame_tree, 802, 1, kDividerActive);
    wlr_scene_node_set_position(&frame->titlebar_divider->node, 0, 28);

    // 4. Subtle outer borders (1px)
    frame->border_top = wlr_scene_rect_create(frame->frame_tree, 802, 1, kBorderActive);
    wlr_scene_node_set_position(&frame->border_top->node, 0, 0);

    frame->border_bottom = wlr_scene_rect_create(frame->frame_tree, 802, 1, kBorderActive);
    wlr_scene_node_set_position(&frame->border_bottom->node, 0, 629);

    frame->border_left = wlr_scene_rect_create(frame->frame_tree, 1, 630, kBorderActive);
    wlr_scene_node_set_position(&frame->border_left->node, 0, 0);

    frame->border_right = wlr_scene_rect_create(frame->frame_tree, 1, 630, kBorderActive);
    wlr_scene_node_set_position(&frame->border_right->node, 801, 0);

    // 5. Traffic lights container (14px left margin, 8px top)
    frame->traffic_tree = wlr_scene_tree_create(frame->frame_tree);
    wlr_scene_node_set_position(&frame->traffic_tree->node, 14, 8);

    build_circle(frame->traffic_tree, 0, 0, kCloseFillActive, kCloseBorderActive, frame->close_rects);
    build_circle(frame->traffic_tree, 20, 0, kMinFillActive, kMinBorderActive, frame->min_rects);
    build_circle(frame->traffic_tree, 40, 0, kMaxFillActive, kMaxBorderActive, frame->max_rects);

    // 6. Client tree container (offset by 1px left, 29px top)
    frame->client_tree = wlr_scene_tree_create(frame->frame_tree);
    wlr_scene_node_set_position(&frame->client_tree->node, 1, 29);

    frame->update_geometry(800, 600);
    frame->set_active(true);

    TinexusWindowFrame* ptr = frame.get();
    m_frames[window_key] = std::move(frame);
    log::info("[Decoration] Created Tinexus SSD frame for window {} ('{}')",
              window_key, label ? label : "unknown");
    return ptr;
}

void TinexusDecorationManager::destroy_frame_impl(void* window_key) {
    auto it = m_frames.find(window_key);
    if (it != m_frames.end()) {
        if (it->second && it->second->frame_tree) {
            wlr_scene_node_destroy(&it->second->frame_tree->node);
        }
        m_frames.erase(it);
        log::info("[Decoration] Destroyed SSD frame for window {}", window_key);
    }
}

TinexusWindowFrame* TinexusDecorationManager::get_frame_impl(void* window_key) const {
    auto it = m_frames.find(window_key);
    return (it != m_frames.end()) ? it->second.get() : nullptr;
}

TinexusWindowFrame* TinexusDecorationManager::create_frame(struct wlr_xdg_toplevel* toplevel, struct wlr_scene_tree* parent) {
    const char* label = (toplevel && toplevel->app_id) ? toplevel->app_id : "wayland-toplevel";
    return create_frame_impl(static_cast<void*>(toplevel), parent, label);
}

void TinexusDecorationManager::destroy_frame(struct wlr_xdg_toplevel* toplevel) {
    destroy_frame_impl(static_cast<void*>(toplevel));
}

TinexusWindowFrame* TinexusDecorationManager::get_frame(struct wlr_xdg_toplevel* toplevel) const {
    return get_frame_impl(static_cast<void*>(toplevel));
}

TinexusWindowFrame* TinexusDecorationManager::create_xwayland_frame(struct wlr_xwayland_surface* xsurface, struct wlr_scene_tree* parent) {
    const char* label = (xsurface && xsurface->c_class) ? xsurface->c_class : "x11-surface";
    return create_frame_impl(static_cast<void*>(xsurface), parent, label);
}

void TinexusDecorationManager::destroy_xwayland_frame(struct wlr_xwayland_surface* xsurface) {
    destroy_frame_impl(static_cast<void*>(xsurface));
}

TinexusWindowFrame* TinexusDecorationManager::get_xwayland_frame(struct wlr_xwayland_surface* xsurface) const {
    return get_frame_impl(static_cast<void*>(xsurface));
}

void TinexusDecorationManager::set_xwayland_active(struct wlr_xwayland_surface* xsurface, bool active) {
    auto* f = get_xwayland_frame(xsurface);
    if (f) f->set_active(active);
}

void TinexusDecorationManager::update_xwayland_geometry(struct wlr_xwayland_surface* xsurface, int32_t w, int32_t h) {
    auto* f = get_xwayland_frame(xsurface);
    if (f) f->update_geometry(w, h);
}

bool TinexusDecorationManager::handle_cursor_button(struct wlr_scene_node* /*node*/, double cursor_x, double cursor_y,
                                                   uint32_t /*button*/, uint32_t state, void* wrapper) {
    if (state != WL_POINTER_BUTTON_STATE_PRESSED || !wrapper || !m_action_handler) {
        return false;
    }

    // Find if the clicked window has an active SSD frame
    TinexusWindowFrame* frame = nullptr;
    void* matched_key = nullptr;

    for (const auto& [key, f] : m_frames) {
        if (f && f->frame_tree) {
            double local_x = cursor_x - f->frame_tree->node.x;
            double local_y = cursor_y - f->frame_tree->node.y;
            HitTarget target = f->hit_test(local_x, local_y);
            if (target != HitTarget::None) {
                frame = f.get();
                matched_key = key;
                break;
            }
        }
    }

    if (!frame || !matched_key) {
        return false;
    }

    double local_x = cursor_x - frame->frame_tree->node.x;
    double local_y = cursor_y - frame->frame_tree->node.y;
    HitTarget target = frame->hit_test(local_x, local_y);

    switch (target) {
        case HitTarget::CloseButton:
            log::info("[Decoration] Hit Close button on window {}", matched_key);
            m_action_handler->request_close(wrapper);
            return true;

        case HitTarget::MinimizeButton:
            log::info("[Decoration] Hit Minimize button on window {}", matched_key);
            m_action_handler->request_minimize(wrapper, true);
            return true;

        case HitTarget::MaximizeButton: {
            bool is_max = m_action_handler->is_window_maximized(wrapper);
            log::info("[Decoration] Hit Maximize button on window {} (currently max={})",
                      matched_key, is_max);
            m_action_handler->request_maximize(wrapper, !is_max);
            return true;
        }

        case HitTarget::TitlebarDrag: {
            // Double click within 350ms triggers maximize toggle
            auto now_ms = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());

            if (m_last_click_wrapper == wrapper && (now_ms - m_last_click_time) <= 350) {
                m_last_click_time = 0;
                m_last_click_wrapper = nullptr;
                bool is_max = m_action_handler->is_window_maximized(wrapper);
                log::info("[Decoration] Double-click on titlebar -> toggle maximize (state={})", !is_max);
                m_action_handler->request_maximize(wrapper, !is_max);
                return true;
            }

            m_last_click_time = now_ms;
            m_last_click_wrapper = wrapper;

            log::info("[Decoration] Initiating titlebar drag for window {}", matched_key);
            m_action_handler->request_move(wrapper);
            return true;
        }

        case HitTarget::BorderTop:
            m_action_handler->request_resize(wrapper, WLR_EDGE_TOP);
            return true;
        case HitTarget::BorderBottom:
            m_action_handler->request_resize(wrapper, WLR_EDGE_BOTTOM);
            return true;
        case HitTarget::BorderLeft:
            m_action_handler->request_resize(wrapper, WLR_EDGE_LEFT);
            return true;
        case HitTarget::BorderRight:
            m_action_handler->request_resize(wrapper, WLR_EDGE_RIGHT);
            return true;

        case HitTarget::None:
        default:
            return false;
    }
}

void TinexusDecorationManager::set_toplevel_active(struct wlr_xdg_toplevel* toplevel, bool active) {
    auto* f = get_frame(toplevel);
    if (f) {
        f->set_active(active);
    }
}

void TinexusDecorationManager::set_toplevel_fullscreen(struct wlr_xdg_toplevel* toplevel, bool fullscreen) {
    auto* f = get_frame(toplevel);
    if (f) {
        f->set_fullscreen(fullscreen);
    }
}

void TinexusDecorationManager::update_toplevel_geometry(struct wlr_xdg_toplevel* toplevel, int32_t w, int32_t h) {
    auto* f = get_frame(toplevel);
    if (f) {
        f->update_geometry(w, h);
    }
}

// ---------------------------------------------------------------------------
// Protocol Signal Handlers
// ---------------------------------------------------------------------------

void TinexusDecorationManager::handle_new_toplevel_decoration(struct wl_listener* listener, void* data) {
    TinexusDecorationManager* self = wl_container_of(listener, self, m_new_decoration_listener);
    auto* decoration = static_cast<struct wlr_xdg_toplevel_decoration_v1*>(data);

    if (!decoration || !decoration->toplevel) return;

    const char* app_id = decoration->toplevel->app_id;
    log::info("[Decoration] New toplevel decoration interface created: toplevel={} app_id='{}'",
              static_cast<void*>(decoration->toplevel), app_id ? app_id : "unknown");

    auto ctx = std::make_unique<DecorationContext>();
    ctx->decoration = decoration;
    ctx->manager = self;

    ctx->request_mode.notify = handle_decoration_request_mode;
    wl_signal_add(&decoration->events.request_mode, &ctx->request_mode);

    ctx->destroy.notify = handle_decoration_destroy;
    wl_signal_add(&decoration->events.destroy, &ctx->destroy);

    // Initial negotiation policy evaluation (honor CSD-preferring apps immediately)
    if (is_native_csd_app(app_id)) {
        log::info("[Decoration] App '{}' is CSD-preferred -> configuring CLIENT_SIDE", app_id ? app_id : "");
        wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    } else if (decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE) {
        log::info("[Decoration] Client '{}' requested CLIENT_SIDE -> configuring CLIENT_SIDE", app_id ? app_id : "");
        wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    } else if (decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
        log::info("[Decoration] Client '{}' requested SERVER_SIDE -> configuring SERVER_SIDE", app_id ? app_id : "");
        wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    self->m_contexts.push_back(std::move(ctx));
    if (self->m_action_handler) {
        self->m_action_handler->on_decoration_mode_changed(decoration->toplevel);
    }
}

void TinexusDecorationManager::handle_decoration_request_mode(struct wl_listener* listener, void* /*data*/) {
    DecorationContext* ctx = wl_container_of(listener, ctx, request_mode);
    struct wlr_xdg_toplevel_decoration_v1* decoration = ctx->decoration;
    if (!decoration || !decoration->toplevel) return;

    const char* app_id = decoration->toplevel->app_id;
    log::info("[Decoration] Client requested mode {} for app_id='{}'",
              static_cast<int>(decoration->requested_mode), app_id ? app_id : "unknown");

    // Immediately commit the mode without waiting for surface commit (KWin negotiation pattern)
    if (is_native_csd_app(app_id)) {
        log::info("[Decoration] App '{}' is CSD-preferred -> honoring CLIENT_SIDE", app_id ? app_id : "");
        wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    } else if (decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE) {
        // Client explicitly requested CLIENT_SIDE (e.g. Firefox, GTK, Chrome)
        log::info("[Decoration] Client '{}' explicitly requested CLIENT_SIDE -> honoring CLIENT_SIDE", app_id ? app_id : "");
        wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    } else {
        // Client explicitly requested SERVER_SIDE or has no CSD capability
        log::info("[Decoration] App '{}' setting SERVER_SIDE", app_id ? app_id : "");
        wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    if (ctx->manager && ctx->manager->m_action_handler) {
        ctx->manager->m_action_handler->on_decoration_mode_changed(decoration->toplevel);
    }
}

void TinexusDecorationManager::handle_decoration_destroy(struct wl_listener* listener, void* /*data*/) {
    DecorationContext* ctx = wl_container_of(listener, ctx, destroy);
    log::info("[Decoration] Decoration destroyed for decoration={}", static_cast<void*>(ctx->decoration));

    if (ctx->request_mode.link.next) {
        wl_list_remove(&ctx->request_mode.link);
        ctx->request_mode.link.next = nullptr;
    }
    if (ctx->destroy.link.next) {
        wl_list_remove(&ctx->destroy.link);
        ctx->destroy.link.next = nullptr;
    }

    if (ctx->manager) {
        auto& list = ctx->manager->m_contexts;
        auto it = std::find_if(list.begin(), list.end(), [ctx](const auto& p) {
            return p.get() == ctx;
        });
        if (it != list.end()) {
            list.erase(it);
        }
    }
}

} // namespace tinexus::comp
