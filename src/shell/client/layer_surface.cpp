#include "layer_surface.hpp"
#include "common/logger.hpp"

static void layer_surface_configure(void* data, struct zwlr_layer_surface_v1* layer_surface,
                                    uint32_t serial, uint32_t width, uint32_t height) {
    auto* surface = static_cast<LayerSurface*>(data);
    surface->handle_configure(serial, width, height);
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* layer_surface) {
    auto* surface = static_cast<LayerSurface*>(data);
    surface->handle_closed();
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

LayerSurface::LayerSurface(ClientState& state) : m_state(state) {}

LayerSurface::~LayerSurface() {
    if (m_layer_surface) zwlr_layer_surface_v1_destroy(m_layer_surface);
    if (m_surface) wl_surface_destroy(m_surface);
}

void LayerSurface::create_top_panel() {
    m_surface = wl_compositor_create_surface(m_state.compositor);
    
    // Top Panel: layer=TOP, namespace="panel"
    m_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        m_state.layer_shell, m_surface, m_state.output, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "panel");
    
    zwlr_layer_surface_v1_add_listener(m_layer_surface, &layer_surface_listener, this);

    // Anchor to TOP, LEFT, RIGHT
    zwlr_layer_surface_v1_set_anchor(m_layer_surface, 
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    
    // Set height to 48px, width 0 (let compositor decide based on anchors)
    zwlr_layer_surface_v1_set_size(m_layer_surface, 0, 48);
    
    // Request 48px exclusive zone so windows don't overlap the panel
    zwlr_layer_surface_v1_set_exclusive_zone(m_layer_surface, 48);
    
    // Set keyboard interactivity to none
    zwlr_layer_surface_v1_set_keyboard_interactivity(m_layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    
    wl_surface_commit(m_surface);
    
    tinexus::log::info("LayerSurface for panel requested (TOP layer)");
}

void LayerSurface::handle_configure(uint32_t serial, uint32_t width, uint32_t height) {
    zwlr_layer_surface_v1_ack_configure(m_layer_surface, serial);
    
    if (width == 0 || height == 0) {
        return; // Compositor hasn't decided yet
    }
    
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        
        // Reallocate buffer on resize
        m_buffer = std::make_unique<ShmSurfaceBuffer>(m_state, m_surface, m_width, m_height);
        
        tinexus::log::info("LayerSurface resized: {}x{}", m_width, m_height);
    }
    
    m_configured = true;
    render();
}

void LayerSurface::handle_closed() {
    tinexus::log::info("LayerSurface closed by compositor");
}

void LayerSurface::render() {
    if (!m_configured || !m_buffer) return;

    m_panel.layout(m_width, m_height);
    
    m_painter.begin(m_buffer.get());
    m_panel.draw(m_painter);
    m_painter.end();
    
    m_buffer->commit();
}
