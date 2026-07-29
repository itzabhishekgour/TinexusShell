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

struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct wl_callback;

namespace txui {

class Window final : public Object {
private:
    uint32 m_id{0};
    uint32 m_width{0};
    uint32 m_height{0};
    std::string m_title;
    WindowState m_state{WindowState::Creating};
    bool m_should_close{false};

    Ref<Widget> m_root_widget{nullptr};
    bool m_frame_ready{true};
    bool m_configured{false};

    std::optional<wayland::WaylandConnection> m_connection;
    std::optional<wayland::WaylandEventLoop> m_event_loop;
    std::optional<wayland::WaylandInput> m_input;
    std::unique_ptr<RenderTarget> m_render_target;

    xdg_wm_base* m_wm_base{nullptr};
    xdg_surface* m_xdg_surface{nullptr};
    xdg_toplevel* m_xdg_toplevel{nullptr};
    wl_callback* m_frame_callback{nullptr};

    CommandBuffer m_command_buffer;
    std::unique_ptr<Painter> m_painter;
    PixmanBackend m_backend;

    std::deque<Event> m_events;
    size_t m_event_queue_capacity{10000}; // Default capacity

    Window(uint32 id, uint32 width, uint32 height, std::string_view title) noexcept;

public:
    ~Window() override;

    // Creates a new production Wayland window. Returns Ref<Window> per intrusive Object ref-counting rule.
    [[nodiscard]] static Ref<Window> create(
        uint32 width, uint32 height, std::string_view title = "Tinexus Application") noexcept;

    // Pops the next pending event from this window's private event queue.
    bool poll_event(Event& out_event) noexcept;

    // Configures the maximum number of events retained before dropping old ones.
    void set_event_queue_capacity(size_t capacity) noexcept { m_event_queue_capacity = capacity; }

    // Blocks waiting for Wayland events from the compositor.
    void wait() noexcept;

    // Sets window title on xdg_toplevel.
    void set_title(std::string_view title) noexcept;

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

    void close() noexcept;

    // Internal Wayland / XDG-Shell hooks
    void on_configure(uint32 width, uint32 height) noexcept;
    void on_close_request() noexcept;
    void on_frame_ready() noexcept;
    void push_event(const Event& event) noexcept;

    void bind_wm_base(xdg_wm_base* wm_base) noexcept { m_wm_base = wm_base; }
};

} // namespace txui
