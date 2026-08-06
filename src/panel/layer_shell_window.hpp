#pragma once

#include <txui/wayland/WaylandConnection.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/widgets/Widget.hpp>
#include <string_view>
#include <memory>

struct zwlr_layer_surface_v1;

namespace tinexus::panel {

class LayerShellWindow {
private:
    uint32_t m_width{0};
    uint32_t m_height{48};
    bool m_configured{false};
    bool m_should_close{false};

    txui::wayland::WaylandConnection m_connection;
    std::unique_ptr<txui::WaylandRenderTarget> m_render_target;
    
    zwlr_layer_surface_v1* m_layer_surface{nullptr};

    txui::Ref<txui::Widget> m_root_widget;
    txui::CommandBuffer m_command_buffer;
    std::unique_ptr<txui::Painter> m_painter;
    txui::PixmanBackend m_backend;

public:
    LayerShellWindow(uint32_t height) noexcept;
    ~LayerShellWindow();

    void set_root_widget(txui::Ref<txui::Widget> root) noexcept;
    
    void show() noexcept;
    int exec() noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool should_close() const noexcept { return m_should_close; }

    // Callbacks
    void on_configure(uint32_t width, uint32_t height) noexcept;
    void on_close_request() noexcept;
    void present() noexcept;

private:
    void setup_layer_surface() noexcept;
};

} // namespace tinexus::panel
