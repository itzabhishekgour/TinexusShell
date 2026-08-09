// Force rebuild to resolve ODR violation
#include <txui/window/Window.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/render/CanvasRenderTarget.hpp>
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

void handle_xdg_toplevel_configure(void* data, struct xdg_toplevel* /*toplevel*/, int32_t width, int32_t height, struct wl_array* /*states*/) {
    auto* win = static_cast<Window*>(data);
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
    m_render_target.reset();
    m_input.reset();
    m_event_loop.reset();
    m_connection.reset();
    m_state = WindowState::Destroyed;
}

Ref<Window> Window::create(uint32 width, uint32 height, std::string_view title, bool layer_shell) noexcept {
    uint32 win_id = s_next_window_id.fetch_add(1, std::memory_order_relaxed);
    auto* raw_win = new Window(width, height, title);
    raw_win->m_id = win_id;
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
                    win->m_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
                        win->m_connection->layer_shell(),
                        target_ptr->surface().surface(),
                        nullptr, // default output
                        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
                        "tinexus-shell"
                    );
                    zwlr_layer_surface_v1_add_listener(win->m_layer_surface, &layer_surface_listener, win.get());
                    zwlr_layer_surface_v1_set_size(win->m_layer_surface, width, height);
                    // Anchor TOP+LEFT+RIGHT: compositor stretches the exclusive zone bar across
                    // the full width. We then set left/right margins to center the pill.
                    // margin = (output_width - pill_width) / 2; we use 1920 as default output
                    // until the configure event arrives with the real output dimensions.
                    zwlr_layer_surface_v1_set_anchor(win->m_layer_surface,
                        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
                    const int32_t side_margin = static_cast<int32_t>((1920 - static_cast<int32_t>(width)) / 2);
                    zwlr_layer_surface_v1_set_margin(win->m_layer_surface, 12, side_margin, 0, side_margin);
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
                            
                            std::string app_id = "io.tinexus.shell";
                            if (win->m_title == "Tinexus Lock") app_id = "tinexus-lock";
                            else if (win->m_title == "Tinexus Launcher") app_id = "tinexus-launcher";
                            else if (win->m_title == "Tinexus Settings") app_id = "tinexus-settings";
                            xdg_toplevel_set_app_id(win->m_xdg_toplevel, app_id.c_str());
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

    win->m_state = WindowState::Running;
    return win;
}

bool Window::poll_event(Event& out_event) noexcept {
    if (m_event_loop.has_value()) {
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

void Window::set_keyboard_interactivity(bool enable) noexcept {
    if (m_layer_surface != nullptr) {
        zwlr_layer_surface_v1_set_keyboard_interactivity(m_layer_surface, enable ? ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND : ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
        if (m_connection.has_value()) {
            auto* wayland_target = dynamic_cast<WaylandRenderTarget*>(m_render_target.get());
            if (wayland_target && wayland_target->surface().surface()) {
                wl_surface_commit(wayland_target->surface().surface());
            }
        }
    }
}

void Window::present(const Rect& damage) noexcept {
    if (!m_render_target || !m_configured) {
        m_command_buffer.clear();
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
    m_configured = true;
    if (width == 0 || height == 0) {
        return;
    }

    // For layer surfaces anchored TOP|LEFT|RIGHT, the compositor sends the
    // full output width as the configure width — use it to keep Aura centered.
    if (m_layer_surface && width > m_width) {
        m_output_width = static_cast<int32_t>(width);
        const int32_t side_margin = (m_output_width - static_cast<int32_t>(m_width)) / 2;
        const int32_t clamped = side_margin > 0 ? side_margin : 0;
        zwlr_layer_surface_v1_set_margin(m_layer_surface, 12, clamped, 0, clamped);
        // Don't update m_width — our actual content width stays at m_width (pill size).
        return;
    }

    if (width != m_width || height != m_height) {
        m_width = width;
        m_height = height;

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

    if (m_render_target) {
        auto* wayland_target = static_cast<WaylandRenderTarget*>(m_render_target.get());
        if (wayland_target->resize(width, height)) {
            if (m_layer_surface) {
                zwlr_layer_surface_v1_set_size(m_layer_surface, width, height);
                // Recompute centering margin after resize so Aura stays top-center.
                // m_output_width defaults to 1920 until configure event updates it.
                const int32_t side_margin = static_cast<int32_t>((m_output_width - static_cast<int32_t>(width)) / 2);
                const int32_t clamped = side_margin > 0 ? side_margin : 0;
                zwlr_layer_surface_v1_set_margin(m_layer_surface, 12, clamped, 0, clamped);
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

} // namespace txui
