#pragma once

#include <txui/core/NonCopyable.hpp>
#include <txui/core/Types.hpp>
#include <txui/input/Event.hpp>
#include <optional>
#include <memory>

struct wl_seat;
struct wl_keyboard;
struct wl_pointer;
struct wl_surface;

namespace txui::wayland {

using EventSink = void(*)(void* ctx, const Event& event) noexcept;

class WaylandInput final : public NonCopyable {
private:
    wl_seat* m_seat{nullptr};
    wl_keyboard* m_keyboard{nullptr};
    wl_pointer* m_pointer{nullptr};

    void* m_sink_ctx{nullptr};
    EventSink m_sink{nullptr};

    uint32 m_active_window_id{0};
    double m_pointer_x{0.0};
    double m_pointer_y{0.0};
    KeyModifier m_active_modifiers{KeyModifier::None};

    explicit WaylandInput(wl_seat* seat) noexcept;

public:
    WaylandInput() = default;
    WaylandInput(WaylandInput&& other) noexcept;
    WaylandInput& operator=(WaylandInput&& other) noexcept;
    ~WaylandInput() noexcept;

    // Creates input handler from given Wayland seat.
    [[nodiscard]] static std::unique_ptr<WaylandInput> create(wl_seat* seat) noexcept;

    void set_event_sink(void* ctx, EventSink sink, uint32 window_id) noexcept;
    void emit_event(const Event& event) noexcept;

    [[nodiscard]] wl_seat* seat() const noexcept { return m_seat; }
    [[nodiscard]] wl_keyboard* keyboard() const noexcept { return m_keyboard; }
    [[nodiscard]] wl_pointer* pointer() const noexcept { return m_pointer; }
    [[nodiscard]] bool is_valid() const noexcept { return m_seat != nullptr; }

    // Internal Wayland protocol callbacks
    void bind_capabilities(uint32 caps) noexcept;
    void set_pointer_coords(double x, double y) noexcept { m_pointer_x = x; m_pointer_y = y; }
    void update_modifiers(uint32 mods_depressed, uint32 mods_latched, uint32 mods_locked) noexcept;
};

// Keycode & Button translation utilities
[[nodiscard]] Key translate_linux_keycode(uint32 keycode) noexcept;
[[nodiscard]] MouseButton translate_linux_button(uint32 button) noexcept;

} // namespace txui::wayland
