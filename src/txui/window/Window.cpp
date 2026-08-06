// Force rebuild to resolve ODR violation
#include <txui/window/Window.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/render/CanvasRenderTarget.hpp>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
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

} // namespace

Window::Window(uint32 id, uint32 width, uint32 height, std::string_view title) noexcept
    : m_id(id),
      m_width(width),
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
    m_render_target.reset();
    m_input.reset();
    m_event_loop.reset();
    m_connection.reset();
    m_state = WindowState::Destroyed;
}

Ref<Window> Window::create(uint32 width, uint32 height, std::string_view title) noexcept {
    uint32 win_id = s_next_window_id.fetch_add(1, std::memory_order_relaxed);
    auto* raw_win = new Window(win_id, width, height, title);
    Ref<Window> win(raw_win);

    auto conn_opt = wayland::WaylandConnection::connect();
    if (conn_opt.has_value() && conn_opt->is_valid() && conn_opt->wm_base() != nullptr) {
        win->m_connection = std::move(conn_opt);
        win->m_event_loop = wayland::WaylandEventLoop(win->m_connection->display());
        win->m_input = wayland::WaylandInput::create(win->m_connection->seat());

        if (win->m_input != nullptr) {
            win->m_input->set_event_sink(win.get(), handle_input_event, win_id);
        }

        win->bind_wm_base(win->m_connection->wm_base());
        xdg_wm_base_add_listener(win->m_wm_base, &wm_base_listener, win.get());

        auto target_opt = WaylandRenderTarget::create(*win->m_connection, width, height);
        if (target_opt.has_value()) {
            auto* target_ptr = new WaylandRenderTarget(std::move(*target_opt));
            win->m_render_target.reset(target_ptr);

            // Create XDG surface & toplevel wrappers for Wayland window
            win->m_xdg_surface = xdg_wm_base_get_xdg_surface(win->m_wm_base, target_ptr->surface().surface());
            if (win->m_xdg_surface != nullptr) {
                xdg_surface_add_listener(win->m_xdg_surface, &xdg_surface_listener, win.get());
                win->m_xdg_toplevel = xdg_surface_get_toplevel(win->m_xdg_surface);
                if (win->m_xdg_toplevel != nullptr) {
                    xdg_toplevel_add_listener(win->m_xdg_toplevel, &xdg_toplevel_listener, win.get());
                    xdg_toplevel_set_title(win->m_xdg_toplevel, win->m_title.c_str());
                    xdg_toplevel_set_app_id(win->m_xdg_toplevel, "io.tinexus.shell");
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

void Window::set_title(std::string_view title) noexcept {
    m_title = title;
    if (m_xdg_toplevel != nullptr) {
        xdg_toplevel_set_title(m_xdg_toplevel, m_title.c_str());
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

} // namespace txui
