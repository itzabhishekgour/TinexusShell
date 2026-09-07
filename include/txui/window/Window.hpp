#pragma once

#include <txui/core/Object.hpp>
#include <txui/core/Ref.hpp>
#include <txui/core/Types.hpp>
#include <txui/input/Event.hpp>
#include <txui/render/RenderTarget.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/wayland/WaylandConnection.hpp>
#include <txui/wayland/WaylandEventLoop.hpp>
#include <txui/wayland/WaylandInput.hpp>
#include <txui/widgets/Widget.hpp>
#include <txui/layout/Constraints.hpp>
#include <deque>
#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <optional>

struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct zwlr_layer_surface_v1;
struct wl_callback;

namespace txui {

namespace wayland {
class WaylandClipboard;
}

enum class LayerType {
    Background,
    Bottom,
    Top,
    Overlay
};

enum LayerAnchor {
    None = 0,
    Top = 1,
    Bottom = 2,
    Left = 4,
    Right = 8
};

class Window final : public Object {
private:
    uint32 m_id{0};
    uint32 m_width{0};
    uint32 m_height{0};
    std::string m_title;
    WindowState m_state{WindowState::Creating};
    bool m_should_close{false};
    bool m_is_maximized{false};

    Ref<Widget> m_root_widget{nullptr};
    bool m_frame_ready{true};
    bool m_configured{false};

    std::optional<wayland::WaylandConnection> m_connection;
    std::optional<wayland::WaylandEventLoop> m_event_loop;
    std::unique_ptr<wayland::WaylandInput> m_input;
    std::unique_ptr<wayland::WaylandClipboard> m_clipboard;
    std::unique_ptr<RenderTarget> m_render_target;

    xdg_wm_base* m_wm_base{nullptr};
    xdg_surface* m_xdg_surface{nullptr};
    xdg_toplevel* m_xdg_toplevel{nullptr};
    zwlr_layer_surface_v1* m_layer_surface{nullptr};
    wl_callback* m_frame_callback{nullptr};
    int32_t m_output_width{1920}; ///< Compositor output width for centering (updated on configure)
    bool m_has_custom_margins{false};
    int32_t m_margin_top{0};
    int32_t m_margin_right{0};
    int32_t m_margin_bottom{0};
    int32_t m_margin_left{0};

    CommandBuffer m_command_buffer;
    std::unique_ptr<Painter> m_painter;
    PixmanBackend m_backend;

    std::deque<Event> m_events;
    size_t m_event_queue_capacity{10000}; // Default capacity

    Window(uint32 width, uint32 height, std::string_view title) noexcept;
    bool init_wayland(bool layer_shell) noexcept;

public:
    ~Window() override;

    // Creates a new production Wayland window. Returns Ref<Window> per intrusive Object ref-counting rule.
    [[nodiscard]] static Ref<Window> create(
        uint32 width, uint32 height, std::string_view title = "Tinexus Application", bool layer_shell = false) noexcept;

    void resize(uint32 width, uint32 height) noexcept;

    // Pops the next pending event from this window's private event queue.
    bool poll_event(Event& out_event) noexcept;

    // Configures the maximum number of events retained before dropping old ones.
    void set_event_queue_capacity(size_t capacity) noexcept { m_event_queue_capacity = capacity; }

    // Blocks waiting for Wayland events from the compositor.
    void wait() noexcept;

    // Blocks waiting for Wayland events up to timeout_ms milliseconds.
    void wait_timeout(int timeout_ms) noexcept;

    // Sets window title on xdg_toplevel.
    void set_title(std::string_view title) noexcept;

    // Set the window to fullscreen mode
    void set_fullscreen(bool fullscreen) noexcept;

    // Window interaction
    void start_interactive_move(uint32 serial) noexcept;
    void start_interactive_resize(uint32 edges, uint32 serial) noexcept;

    // Enable or disable keyboard interactivity for layer shell surfaces
    void set_keyboard_interactivity(bool enable) noexcept;
    
    // Configure layer shell anchors and exclusive zone. Must be called before wait() or create() if possible.
    // Actually, can be called on a created window.
    void set_layer_shell_config(LayerType layer, uint32_t anchors, int32_t exclusive_zone) noexcept;
    void set_layer_margins(int32_t top, int32_t right, int32_t bottom, int32_t left) noexcept;
    
    // Set tick callback to run in the event loop every frame
    void set_tick_callback(std::function<void()> cb) noexcept;

    // Exposes Painter API for rendering commands.
    [[nodiscard]] Painter& painter() noexcept { return *m_painter; }

    // Executes recorded painter commands and commits back buffer to Wayland surface.
    void present(const Rect& damage = Rect()) noexcept;

    [[nodiscard]] uint32 id() const noexcept { return m_id; }
    [[nodiscard]] uint32 width() const noexcept { return m_width; }
    [[nodiscard]] uint32 height() const noexcept { return m_height; }
    [[nodiscard]] WindowState state() const noexcept { return m_state; }
    [[nodiscard]] bool should_close() const noexcept { return m_should_close; }

    void set_root_widget(Ref<Widget> root) noexcept {
        m_root_widget = std::move(root);
        if (m_root_widget) {
            m_root_widget->mark_needs_measure();
        }
    }

    [[nodiscard]] Ref<Widget> root_widget() const noexcept { return m_root_widget; }

    [[nodiscard]] bool is_wayland_connected() const noexcept { return m_connection.has_value() && m_connection->is_valid(); }
    
    // Expose the event loop for adding custom FDs
    [[nodiscard]] wayland::WaylandEventLoop* event_loop() noexcept { return m_event_loop ? &*m_event_loop : nullptr; }

    // Set or unset maximized state (sends xdg_toplevel request to compositor)
    void set_maximized(bool maximized) noexcept;

    // Minimize (hide) the window. No restore path until a dock exists.
    void minimize() noexcept;

    // Returns true if the window is currently in maximized state.
    [[nodiscard]] bool is_maximized() const noexcept { return m_is_maximized; }

    void close() noexcept;

    // Clipboard interaction
    bool set_clipboard_text(std::string_view text) noexcept;
    [[nodiscard]] std::string get_clipboard_text() noexcept;
    [[nodiscard]] bool has_clipboard_text() const noexcept;

    // Internal Wayland / XDG-Shell hooks
    void on_configure(uint32 width, uint32 height) noexcept;
    void on_close_request() noexcept;
    void on_frame_ready() noexcept;
    void push_event(const Event& event) noexcept;

    void bind_wm_base(xdg_wm_base* wm_base) noexcept { m_wm_base = wm_base; }
    
private:
    std::function<void()> m_tick_callback;
};

} // namespace txui
