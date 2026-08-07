#include "layer_shell_window.hpp"
#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <wayland-client.h>
#include <common/logger.hpp>
#include <txui/render/PixmanBackend.hpp>

namespace tinexus::panel {

static zwlr_layer_shell_v1* g_layer_shell = nullptr;

static void registry_handle_global(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (std::string_view(interface) == zwlr_layer_shell_v1_interface.name) {
        g_layer_shell = static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version >= 4 ? 4 : version));
        log::info("LayerShellWindow: Bound zwlr_layer_shell_v1 global");
    }
}

static void registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
    // Optional handling
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void layer_surface_configure(void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width, uint32_t height) {
    auto* window = static_cast<LayerShellWindow*>(data);
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    window->on_configure(width, height);
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface) {
    auto* window = static_cast<LayerShellWindow*>(data);
    window->on_close_request();
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

LayerShellWindow::LayerShellWindow(uint32_t height) noexcept : m_height(height) {
    // 1. Connect to Wayland to get base globals (compositor, shm, etc.)
    auto conn_opt = txui::wayland::WaylandConnection::connect();
    if (!conn_opt) {
        log::error("LayerShellWindow: Failed to connect to Wayland");
        return;
    }
    m_connection = std::move(*conn_opt);

    // 2. Bind layer shell
    wl_registry* reg = wl_display_get_registry(m_connection.display());
    wl_registry_add_listener(reg, &registry_listener, this);
    wl_display_roundtrip(m_connection.display());

    if (!g_layer_shell) {
        log::error("LayerShellWindow: Compositor does not support zwlr_layer_shell_v1!");
        return;
    }

    // 3. Create RenderTarget
    // Assume full screen width for now (e.g. 1920) or wait for configure
    m_width = 1920; 
    auto target_opt = txui::WaylandRenderTarget::create(m_connection, m_width, m_height);
    if (!target_opt) {
        log::error("LayerShellWindow: Failed to create WaylandRenderTarget");
        return;
    }
    m_render_target = std::make_unique<txui::WaylandRenderTarget>(std::move(*target_opt));

    m_painter = std::make_unique<txui::Painter>(m_command_buffer);

    setup_layer_surface();
}

LayerShellWindow::~LayerShellWindow() {
    if (m_layer_surface) {
        zwlr_layer_surface_v1_destroy(m_layer_surface);
    }
}

void LayerShellWindow::setup_layer_surface() noexcept {
    wl_surface* raw_surface = m_render_target->surface().surface();
    
    // Create layer surface anchored to the top
    m_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        g_layer_shell, raw_surface, nullptr,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP, "tinexus-panel");

    zwlr_layer_surface_v1_add_listener(m_layer_surface, &layer_surface_listener, this);
    
    zwlr_layer_surface_v1_set_size(m_layer_surface, m_width, m_height);
    zwlr_layer_surface_v1_set_anchor(m_layer_surface, 
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | 
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | 
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(m_layer_surface, static_cast<int32_t>(m_height));
    
    // Commit to trigger configure event
    wl_surface_commit(raw_surface);
    m_connection.flush();
}

void LayerShellWindow::set_root_widget(txui::Ref<txui::Widget> root) noexcept {
    m_root_widget = std::move(root);
    if (m_root_widget) {
        m_root_widget->mark_needs_measure();
    }
}

void LayerShellWindow::on_configure(uint32_t width, uint32_t height) noexcept {
    if (width > 0) m_width = width;
    if (height > 0) m_height = height;

    if (m_width != m_render_target->width() || m_height != m_render_target->height()) {
        m_render_target->resize(m_width, m_height);
    }

    m_configured = true;
    present();
}

void LayerShellWindow::on_close_request() noexcept {
    m_should_close = true;
}

void LayerShellWindow::present() noexcept {
    if (!is_valid() || !m_configured || !m_root_widget || m_should_close) return;

    // 1. Measure & Layout
    txui::Constraints constraints;
    constraints.min_width = constraints.max_width = m_width;
    constraints.min_height = constraints.max_height = m_height;
    
    m_root_widget->measure(constraints);
    m_root_widget->layout(txui::Rect(0, 0, m_width, m_height));

    // 2. Paint
    m_painter->begin_frame();
    m_painter->fill_rect(txui::Rect(0, 0, m_width, m_height), txui::Color(0, 0, 0, 127)); // Base semi-transparent background
    m_root_widget->paint(*m_painter);
    m_painter->end_frame();

    // 3. Render and Commit
    m_backend.execute(m_command_buffer, *m_render_target);
    m_command_buffer.clear();
    
    m_render_target->present();
    m_connection.flush();
}

void LayerShellWindow::show() noexcept {
    if (!is_valid()) return;
    // Initial roundtrip to process configure
    m_connection.roundtrip();
}

#include <poll.h>

int LayerShellWindow::exec() noexcept {
    if (!is_valid()) {
        return -1;
    }
    
    int fd = wl_display_get_fd(m_connection.display());
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (!m_should_close) {
        while (wl_display_prepare_read(m_connection.display()) != 0) {
            wl_display_dispatch_pending(m_connection.display());
        }
        wl_display_flush(m_connection.display());

        int ret = poll(&pfd, 1, 1000); // 1 second timeout
        if (ret > 0) {
            if (wl_display_read_events(m_connection.display()) == -1) {
                m_should_close = true;
                break;
            }
            if (wl_display_dispatch_pending(m_connection.display()) == -1) {
                m_should_close = true;
                break;
            }
        } else {
            wl_display_cancel_read(m_connection.display());
            if (ret < 0) {
                m_should_close = true;
                break;
            }
        }
        
        // Return control briefly to allow main thread to process timer
        break; 
    }
    return 0;
}

bool LayerShellWindow::is_valid() const noexcept {
    return m_connection.is_valid() && m_layer_surface != nullptr && m_render_target != nullptr;
}

} // namespace tinexus::panel
