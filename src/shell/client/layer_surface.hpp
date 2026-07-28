#pragma once

#include "client_state.hpp"
#include "../ui/panel_widget.hpp"
#include "../ui/painter.hpp"
#include "../render/shm_surface_buffer.hpp"
#include <memory>

class LayerSurface {
public:
    LayerSurface(ClientState& state);
    ~LayerSurface();

    void create_top_panel();
    void handle_configure(uint32_t serial, uint32_t width, uint32_t height);
    void handle_closed();

private:
    void render();

    ClientState& m_state;
    wl_surface* m_surface{nullptr};
    zwlr_layer_surface_v1* m_layer_surface{nullptr};
    
    std::unique_ptr<ShmSurfaceBuffer> m_buffer;
    ui::ShmPainter m_painter;
    ui::PanelWidget m_panel;

    uint32_t m_width{0};
    uint32_t m_height{0};
    bool m_configured{false};
};
