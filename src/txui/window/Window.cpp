#include <txui/window/Window.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/render/CanvasRenderTarget.hpp>
#include <txui/wayland/WaylandClipboard.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <atomic>
#include <utility>

namespace txui {

namespace {

std::atomic<uint32> s_next_window_id{1};

void handle_xdg_wm_base_ping(void* /*data*/, struct xdg_wm_base* wm_base, uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
}

const struct xdg_wm_base_listener wm_base_listener = {
    .ping = handle_xdg_wm_base_ping
};

void handle_xdg_surface_configure(void* data, struct xdg_surface* xdg_surface, uint32_t serial) {
    auto* win = static_cast<Window*>(data);
    xdg_surface_ack_configure(xdg_surface, serial);
    win->on_configure(win->width(), win->height());
}

const struct xdg_surface_listener xdg_surface_listener = {
    .configure = handle_xdg_surface_configure
};

void handle_xdg_toplevel_configure(void* data, struct xdg_toplevel* /*toplevel*/, int32_t width, int32_t height, struct wl_array* states) {
    auto* win = static_cast<Window*>(data);
    bool is_max = false;
    bool is_fullscreen = false;
    bool is_activated = false;
    bool is_tiled = false;

    if (states != nullptr && states->data != nullptr) {
        uint32_t* state;
        for (state = static_cast<uint32_t*>(states->data);
             reinterpret_cast<const char*>(state) < (reinterpret_cast<const char*>(states->data) + states->size);
             state++) {
            if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED) is_max = true;
            else if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) is_fullscreen = true;
            else if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) is_activated = true;
            else if (*state == XDG_TOPLEVEL_STATE_TILED_LEFT ||
                     *state == XDG_TOPLEVEL_STATE_TILED_RIGHT ||
                     *state == XDG_TOPLEVEL_STATE_TILED_TOP ||
                     *state == XDG_TOPLEVEL_STATE_TILED_BOTTOM) {
                is_tiled = true;
            }
        }
    }

    win->set_xdg_states(is_max, is_fullscreen, is_activated, is_tiled);

    if (width > 0 && height > 0) {
        win->on_configure(static_cast<uint32>(width), static_cast<uint32>(height));
    }
}

void handle_xdg_toplevel_close(void* data, struct xdg_toplevel* /*toplevel*/) {
    auto* win = static_cast<Window*>(data);
    win->on_close_request();
}

void handle_xdg_toplevel_configure_bounds(void* /*data*/, struct xdg_toplevel* /*toplevel*/, int32_t /*width*/, int32_t /*height*/) {}
void handle_xdg_toplevel_wm_capabilities(void* /*data*/, struct xdg_toplevel* /*toplevel*/, struct wl_array* /*capabilities*/) {}

const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = handle_xdg_toplevel_configure,
    .close = handle_xdg_toplevel_close,
    .configure_bounds = handle_xdg_toplevel_configure_bounds,
    .wm_capabilities = handle_xdg_toplevel_wm_capabilities
};

void handle_frame_done(void* data, struct wl_callback* callback, uint32_t /*time*/);

const struct wl_callback_listener frame_listener = {
    .done = handle_frame_done
};

void handle_frame_done(void* data, struct wl_callback* callback, uint32_t /*time*/) {
    auto* win = static_cast<Window*>(data);
    wl_callback_destroy(callback);
    win->on_frame_ready();
}

void handle_input_event(void* ctx, const Event& event) noexcept {
    auto* win = static_cast<Window*>(ctx);
    win->push_event(event);
}

static void handle_layer_surface_configure(void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width, uint32_t height) {
    auto* win = static_cast<Window*>(data);
    if (width > 0 && height > 0) {
        win->on_configure(width, height);
    }
    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void handle_layer_surface_closed(void* data, struct zwlr_layer_surface_v1* /*surface*/) {
    auto* win = static_cast<Window*>(data);
    win->on_close_request();
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = handle_layer_surface_configure,
    .closed = handle_layer_surface_closed,
};

} // namespace

Window::Window(uint32 width, uint32 height, std::string_view title) noexcept
    : m_width(width),
      m_height(height),
      m_title(title),
      m_painter(std::make_unique<Painter>(m_command_buffer)) {}

Window::~Window() {
    if (m_frame_callback != nullptr) {
        wl_callback_destroy(m_frame_callback);
        m_frame_callback = nullptr;
    }
    if (m_xdg_toplevel != nullptr) {
        xdg_toplevel_destroy(m_xdg_toplevel);
        m_xdg_toplevel = nullptr;
    }
    if (m_xdg_surface != nullptr) {
        xdg_surface_destroy(m_xdg_surface);
        m_xdg_surface = nullptr;
    }
    if (m_layer_surface != nullptr) {
        zwlr_layer_surface_v1_destroy(m_layer_surface);
        m_layer_surface = nullptr;
    }
    m_clipboard.reset();
    m_render_target.reset();
    m_input.reset();
    m_event_loop.reset();
    m_connection.reset();
    m_state = WindowState::Destroyed;
}

Ref<Window> Window::create(uint32 width, uint32 height, std::string_view title, bool layer_shell, std::string_view app_id) noexcept {
    uint32 win_id = s_next_window_id.fetch_add(1, std::memory_order_relaxed);
    auto* raw_win = new Window(width, height, title);
    raw_win->m_id = win_id;
    if (!app_id.empty()) {
        raw_win->m_app_id = app_id;
    }
    Ref<Window> win(raw_win);

    auto conn_opt = wayland::WaylandConnection::connect();
    if (conn_opt.has_value() && conn_opt->is_valid()) {
        win->m_connection = std::move(conn_opt);
        win->m_event_loop = wayland::WaylandEventLoop(win->m_connection->display());
        win->m_input = wayland::WaylandInput::create(win->m_connection->seat());

        if (win->m_input != nullptr) {
            win->m_input->set_event_sink(win.get(), handle_input_event, win_id);
        }
        
        // CRITICAL FIX: Ensure wl_seat capabilities are fully resolved so wl_keyboard
        // is bound *before* we map the window and the compositor sends keyboard.enter.
        win->m_connection->roundtrip();

        auto target_opt = WaylandRenderTarget::create(*win->m_connection, width, height);
        if (target_opt.has_value()) {
            auto* target_ptr = new WaylandRenderTarget(std::move(*target_opt));
            win->m_render_target.reset(target_ptr);

            if (layer_shell) {
                if (win->m_connection->layer_shell() != nullptr) {
                    const char* ns = win->m_title.c_str();
                    if (win->m_title == "Aura") {
                        ns = "tinexus-shell";
                    } else if (win->m_title == "dock") {
                        ns = "tinexus-dock";
                    }
                    win->m_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
                        win->m_connection->layer_shell(),
                        target_ptr->surface().surface(),
                        nullptr, // default output
                        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
                        ns
                    );
                    zwlr_layer_surface_v1_add_listener(win->m_layer_surface, &layer_surface_listener, win.get());
                    zwlr_layer_surface_v1_set_size(win->m_layer_surface, width, height);
                    // Anchor TOP+LEFT+RIGHT: compositor stretches the exclusive zone bar across
                    // the full width. We then set left/right margins to center the pill.
                    // margin = (output_width - pill_width) / 2; we use 1920 as default output
                    // until the configure event arrives with the real output dimensions.
                    // Use stored config or default to TOP + exclusive zone
                    zwlr_layer_surface_v1_set_anchor(win->m_layer_surface,
                        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
                    const int32_t side_margin = static_cast<int32_t>((1920 - static_cast<int32_t>(width)) / 2);
                    const int32_t top_margin = (win->m_title == "Aura" || win->m_title == "TopBar" || win->m_title == "shell" || win->m_title == "tinexus-shell") ? 0 : 12;
                    zwlr_layer_surface_v1_set_margin(win->m_layer_surface, top_margin, side_margin > 0 ? side_margin : 0, 0, side_margin > 0 ? side_margin : 0);
                    zwlr_layer_surface_v1_set_keyboard_interactivity(win->m_layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
                    zwlr_layer_surface_v1_set_exclusive_zone(win->m_layer_surface, -1);
                }
            } else {
                if (win->m_connection->wm_base() != nullptr) {
                    xdg_wm_base_add_listener(win->m_connection->wm_base(), &wm_base_listener, win.get());

                    win->m_xdg_surface = xdg_wm_base_get_xdg_surface(win->m_connection->wm_base(), target_ptr->surface().surface());
                    if (win->m_xdg_surface != nullptr) {
                        xdg_surface_add_listener(win->m_xdg_surface, &xdg_surface_listener, win.get());
                        win->m_xdg_toplevel = xdg_surface_get_toplevel(win->m_xdg_surface);
                        if (win->m_xdg_toplevel != nullptr) {
                            xdg_toplevel_add_listener(win->m_xdg_toplevel, &xdg_toplevel_listener, win.get());
                            xdg_toplevel_set_title(win->m_xdg_toplevel, win->m_title.c_str());
                            
                            std::string effective_app_id = "io.tinexus.shell";
                            if (!win->m_app_id.empty()) {
                                effective_app_id = win->m_app_id;
                            } else if (win->m_title == "Tinexus Lock") effective_app_id = "tinexus-lock";
                            else if (win->m_title == "Tinexus Launcher") effective_app_id = "tinexus-launcher";
                            else if (win->m_title == "Tinexus Settings") effective_app_id = "tinexus-settings";
                            else if (win->m_title == "About Tinexus" || win->m_title == "Tinexus About") effective_app_id = "tinexus-about";
                            else if (win->m_title == "Activity Monitor" || win->m_title == "Tinexus Activity Monitor") effective_app_id = "tinexus-monitor";
                            else if (win->m_title == "App Store" || win->m_title == "Tinexus Store") effective_app_id = "tinexus-store";
                            else if (win->m_title == "Tinexus Terminal") effective_app_id = "tinexus-terminal";
                            else if (win->m_title == "tinexus-files" || win->m_title == "Tinexus Files") effective_app_id = "tinexus-files";
                            win->m_app_id = effective_app_id;
                            xdg_toplevel_set_app_id(win->m_xdg_toplevel, effective_app_id.c_str());
                        }
                    }
                }
            }

            wl_surface_commit(target_ptr->surface().surface());
            win->m_connection->roundtrip();
        }
    }

    if (!win->m_render_target) {
        // Offline / Headless fallback for automated CI testing
        win->m_render_target = std::make_unique<CanvasRenderTarget>(width, height);
    }

    if (win->m_connection.has_value() && win->m_connection->is_valid()) {
        win->m_clipboard = std::make_unique<wayland::WaylandClipboard>(*win->m_connection);
    }

    win->m_state = WindowState::Running;
    return win;
}

bool Window::poll_event(Event& out_event) noexcept {
    if (m_events.empty() && m_event_loop.has_value()) {
        m_event_loop->poll();
    }
    if (!m_events.empty()) {
        out_event = m_events.front();
        m_events.pop_front();
        return true;
    }
    return false;
}

void Window::wait() noexcept {
    if (m_event_loop.has_value()) {
        m_event_loop->wait();
    }
}

void Window::wait_timeout(int timeout_ms) noexcept {
    if (m_event_loop.has_value()) {
        m_event_loop->wait_timeout(timeout_ms);
    }
}

void Window::set_title(std::string_view title) noexcept {
    m_title = title;
    if (m_xdg_toplevel != nullptr) {
        xdg_toplevel_set_title(m_xdg_toplevel, m_title.c_str());
    }
}

void Window::set_fullscreen(bool fullscreen) noexcept {
    if (m_xdg_toplevel != nullptr) {
        if (fullscreen) {
            xdg_toplevel_set_fullscreen(m_xdg_toplevel, nullptr);
        } else {
            xdg_toplevel_unset_fullscreen(m_xdg_toplevel);
        }
    }
}

void Window::start_interactive_move(uint32 serial) noexcept {
    if (m_xdg_toplevel != nullptr && m_connection && m_connection->seat()) {
        xdg_toplevel_move(m_xdg_toplevel, m_connection->seat(), serial);
    }
}

void Window::start_interactive_resize(uint32 edges, uint32 serial) noexcept {
    if (m_xdg_toplevel != nullptr && m_connection && m_connection->seat()) {
        xdg_toplevel_resize(m_xdg_toplevel, m_connection->seat(), serial, edges);
    }
}

void Window::set_keyboard_interactivity(bool enable) noexcept {
    if (m_layer_surface) {
        zwlr_layer_surface_v1_set_keyboard_interactivity(m_layer_surface, 
            enable ? ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND 
                   : ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    }
}

void Window::set_layer_shell_config(LayerType layer, uint32_t anchors, int32_t exclusive_zone) noexcept {
    if (m_layer_surface) {
        uint32_t wl_anchors = 0;
        if (anchors & LayerAnchor::Top) wl_anchors |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
        if (anchors & LayerAnchor::Bottom) wl_anchors |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
        if (anchors & LayerAnchor::Left) wl_anchors |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
        if (anchors & LayerAnchor::Right) wl_anchors |= ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        
        zwlr_layer_surface_v1_set_anchor(m_layer_surface, wl_anchors);
        zwlr_layer_surface_v1_set_exclusive_zone(m_layer_surface, exclusive_zone);
        
        if (!m_has_custom_margins) {
            if ((anchors & LayerAnchor::Bottom) && !(anchors & LayerAnchor::Top)) {
                zwlr_layer_surface_v1_set_margin(m_layer_surface, 0, 0, 12, 0); 
            }
        }

        if (m_render_target) {
            auto* wayland_target = dynamic_cast<WaylandRenderTarget*>(m_render_target.get());
            if (wayland_target && wayland_target->surface().surface()) {
                wl_surface_commit(wayland_target->surface().surface());
            }
        }
        if (m_connection.has_value()) {
            m_connection->flush();
        }
    }
}

void Window::set_xdg_states(bool maximized, bool fullscreen, bool activated, bool tiled) noexcept {
    m_is_maximized = maximized;
    m_is_fullscreen = fullscreen;
    m_is_activated = activated;
    m_is_tiled = tiled;

    if (m_root_widget) {
        auto* chrome = dynamic_cast<ChromeWidget*>(m_root_widget.get());
        if (chrome) {
            chrome->set_maximized(maximized || fullscreen || tiled);
        }
    }
}

void Window::set_layer_margins(int32_t top, int32_t right, int32_t bottom, int32_t left) noexcept {
    m_has_custom_margins = true;
    m_margin_top = top;
    m_margin_right = right;
    m_margin_bottom = bottom;
    m_margin_left = left;
    if (m_layer_surface) {
        zwlr_layer_surface_v1_set_margin(m_layer_surface, top, right, bottom, left);
    }
}

void Window::set_tick_callback(std::function<void()> cb) noexcept {
    m_tick_callback = std::move(cb);
}

void Window::present(const Rect& damage) noexcept {
    if (!m_render_target || !m_configured) {
        m_command_buffer.clear();
        return;
    }

    if (m_tick_callback) {
        m_tick_callback();
    }

    if (!m_needs_repaint || !m_frame_ready) {
        return;
    }

    if (m_root_widget != nullptr) {
        if (m_root_widget->needs_measure()) {
            m_root_widget->measure(Constraints::tight(static_cast<float64>(m_width), static_cast<float64>(m_height)));
        }
        if (m_root_widget->needs_layout()) {
            m_root_widget->layout(Rect(0.0, 0.0, static_cast<float64>(m_width), static_cast<float64>(m_height)));
        }
        
        m_render_target->clear();
        m_command_buffer.clear();
        Painter painter(m_command_buffer);
        painter.begin_frame();
        m_root_widget->paint(painter);
        painter.end_frame();
    }

    m_backend.execute(m_command_buffer, *m_render_target);
    m_command_buffer.clear();
    m_frame_ready = false;
    m_needs_repaint = false;

    if (m_connection.has_value()) {
        auto* wayland_target = dynamic_cast<WaylandRenderTarget*>(m_render_target.get());
        if (wayland_target != nullptr) {
            wl_surface* surf = wayland_target->surface().surface();
            if (surf != nullptr && m_frame_callback == nullptr) {
                m_frame_callback = wl_surface_frame(surf);
                if (m_frame_callback != nullptr) {
                    wl_callback_add_listener(m_frame_callback, &frame_listener, this);
                }
            }
            wayland_target->present(damage);
        }
        m_connection->flush();
    }
}

void Window::close() noexcept {
    m_should_close = true;
    m_state = WindowState::Closing;
}

void Window::on_configure(uint32 width, uint32 height) noexcept {
    const bool was_configured = m_configured;
    m_configured = true;

    if (width == 0 || height == 0) {
        return;
    }

    // For layer surfaces anchored TOP|LEFT|RIGHT, the compositor sends the
    // full output width as the configure width — use it to keep Aura centered.
    if (m_layer_surface && width > m_width) {
        if (m_output_width != static_cast<int32_t>(width)) {
            m_output_width = static_cast<int32_t>(width);
            const int32_t side_margin = (m_output_width - static_cast<int32_t>(m_width)) / 2;
            const int32_t clamped = side_margin > 0 ? side_margin : 0;
            zwlr_layer_surface_v1_set_margin(m_layer_surface, 12, clamped, 0, clamped);
            m_needs_repaint = true;
        } else if (!was_configured) {
            m_needs_repaint = true;
        }
        // Don't update m_width — our actual content width stays at m_width (pill size).
        return;
    }

    if (!was_configured || width != m_width || height != m_height) {
        m_width = width;
        m_height = height;
        m_needs_repaint = true;

        if (m_root_widget != nullptr) {
            m_root_widget->mark_needs_measure();
            m_root_widget->mark_needs_layout();
        }

        if (m_render_target) {
            auto* wayland_target = dynamic_cast<WaylandRenderTarget*>(m_render_target.get());
            if (wayland_target != nullptr) {
                wayland_target->resize(width, height);
            } else {
                auto* canvas_target = dynamic_cast<CanvasRenderTarget*>(m_render_target.get());
                if (canvas_target != nullptr) {
                    *canvas_target = CanvasRenderTarget(width, height);
                }
            }
        }

        Event ev;
        ev.type = EventType::WindowResize;
        ev.window_id = m_id;
        ev.resize.width = width;
        ev.resize.height = height;
        push_event(ev);
    }
}

void Window::on_close_request() noexcept {
    m_should_close = true;
    m_state = WindowState::Closing;

    Event ev;
    ev.type = EventType::WindowClose;
    ev.window_id = m_id;
    ev.close.requested = true;
    push_event(ev);
}

void Window::on_frame_ready() noexcept {
    m_frame_callback = nullptr;
    m_frame_ready = true;

    Event ev;
    ev.type = EventType::FrameReady;
    ev.window_id = m_id;
    push_event(ev);
}

void Window::push_event(const Event& event) noexcept {
    if (event.type != EventType::FrameReady) {
        m_needs_repaint = true;
    }

    if (m_event_queue_capacity > 0) {
        while (m_events.size() >= m_event_queue_capacity) {
            m_events.pop_front(); // Drop oldest
        }
        m_events.push_back(event);
    }
}

void Window::resize(uint32_t width, uint32_t height) noexcept {
    if (m_width == width && m_height == height) return;
    m_width = width;
    m_height = height;
    m_needs_repaint = true;

    if (m_render_target) {
        auto* wayland_target = static_cast<WaylandRenderTarget*>(m_render_target.get());
        if (wayland_target->resize(width, height)) {
            if (m_layer_surface) {
                zwlr_layer_surface_v1_set_size(m_layer_surface, width, height);
                // Recompute centering margin after resize so Aura stays top-center.
                // m_output_width defaults to 1920 until configure event updates it.
                const int32_t side_margin = static_cast<int32_t>((m_output_width - static_cast<int32_t>(width)) / 2);
                const int32_t clamped = side_margin > 0 ? side_margin : 0;
                const int32_t top_margin = (m_title == "Aura" || m_title == "TopBar" || m_title == "shell" || m_title == "tinexus-shell") ? 0 : 12;
                if (m_has_custom_margins) {
                    zwlr_layer_surface_v1_set_margin(m_layer_surface, m_margin_top, m_margin_right, m_margin_bottom, m_margin_left);
                } else if (m_title == "dock") {
                    zwlr_layer_surface_v1_set_margin(m_layer_surface, 0, clamped, 12, clamped);
                } else {
                    zwlr_layer_surface_v1_set_margin(m_layer_surface, top_margin, clamped, 0, clamped);
                }
                // NOTE: Do NOT commit here. The size/margin changes are Wayland
                // pending state that will be atomically applied with the next pixel
                // buffer commit in Window::present(). Committing here causes a
                // duplicate commit per animation frame (double commit.notify in logs)
                // and wastes compositor work on an empty/old buffer.
            }
            // xdg_toplevel resize is driven by compositor configure events, not client commits.
        }
    }
    // Trigger full measure+layout+paint so widget tree adapts to new bounds.
    m_frame_ready = true;
    if (m_root_widget) {
        m_root_widget->mark_needs_measure();
        m_root_widget->mark_needs_layout();
        m_root_widget->mark_needs_paint();
    }
}

void Window::set_maximized(bool maximized) noexcept {
    if (m_xdg_toplevel == nullptr) return;
    if (maximized) {
        xdg_toplevel_set_maximized(m_xdg_toplevel);
    } else {
        xdg_toplevel_unset_maximized(m_xdg_toplevel);
    }
    m_is_maximized = maximized;
    if (m_connection.has_value()) {
        m_connection->flush();
    }
}

void Window::minimize() noexcept {
    if (m_xdg_toplevel == nullptr) return;
    xdg_toplevel_set_minimized(m_xdg_toplevel);
    if (m_connection.has_value()) {
        m_connection->flush();
    }
}

bool Window::set_clipboard_text(std::string_view text) noexcept {
    if (m_clipboard) {
        return m_clipboard->set_text(text);
    }
    return false;
}

std::string Window::get_clipboard_text() noexcept {
    if (m_clipboard) {
        return m_clipboard->get_text();
    }
    return "";
}

bool Window::has_clipboard_text() const noexcept {
    if (m_clipboard) {
        return m_clipboard->has_text();
    }
    return false;
}

} // namespace txui
