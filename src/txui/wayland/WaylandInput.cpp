// Force rebuild to resolve ODR violation
#include <txui/wayland/WaylandInput.hpp>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <utility>
#include <memory>

namespace txui::wayland {

namespace {

void keyboard_handle_keymap(void* /*data*/, struct wl_keyboard* /*keyboard*/, uint32_t /*format*/, int32_t fd, uint32_t /*size*/) {
    // Close keymap fd as required by Wayland protocol
    if (fd >= 0) {
        close(fd);
    }
}

void keyboard_handle_enter(void* /*data*/, struct wl_keyboard* /*keyboard*/, uint32_t /*serial*/, struct wl_surface* /*surface*/, struct wl_array* /*keys*/) {
    // Focus entered window
}

void keyboard_handle_leave(void* /*data*/, struct wl_keyboard* /*keyboard*/, uint32_t /*serial*/, struct wl_surface* /*surface*/) {
    // Focus left window
}

void keyboard_handle_key(void* data, struct wl_keyboard* /*keyboard*/, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    auto* input = static_cast<WaylandInput*>(data);
    Event event;
    event.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? EventType::KeyDown : EventType::KeyUp;
    event.timestamp_ns = static_cast<uint64>(time) * 1000000ULL;
    event.keyboard.key = translate_linux_keycode(key);
    event.keyboard.is_repeat = false;
    event.keyboard.serial = serial;
    input->emit_event(event);
}

void keyboard_handle_modifiers(void* data, struct wl_keyboard* /*keyboard*/, uint32_t /*serial*/, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t /*group*/) {
    auto* input = static_cast<WaylandInput*>(data);
    input->update_modifiers(mods_depressed, mods_latched, mods_locked);
}

void keyboard_handle_repeat_info(void* /*data*/, struct wl_keyboard* /*keyboard*/, int32_t /*rate*/, int32_t /*delay*/) {
    // Repeat info updated
}

const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info
};

void pointer_handle_enter(void* data, struct wl_pointer* /*pointer*/, uint32_t serial, struct wl_surface* /*surface*/, wl_fixed_t sx, wl_fixed_t sy) {
    auto* input = static_cast<WaylandInput*>(data);
    double x = wl_fixed_to_double(sx);
    double y = wl_fixed_to_double(sy);
    input->set_pointer_coords(x, y);

    Event event;
    event.type = EventType::PointerEnter;
    event.pointer.x = x;
    event.pointer.y = y;
    event.pointer.serial = serial;
    input->emit_event(event);
}

void pointer_handle_leave(void* data, struct wl_pointer* /*pointer*/, uint32_t /*serial*/, struct wl_surface* /*surface*/) {
    auto* input = static_cast<WaylandInput*>(data);
    Event event;
    event.type = EventType::PointerLeave;
    input->emit_event(event);
}

void pointer_handle_motion(void* data, struct wl_pointer* /*pointer*/, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    auto* input = static_cast<WaylandInput*>(data);
    double x = wl_fixed_to_double(sx);
    double y = wl_fixed_to_double(sy);
    input->set_pointer_coords(x, y);

    Event event;
    event.type = EventType::PointerMove;
    event.timestamp_ns = static_cast<uint64>(time) * 1000000ULL;
    event.pointer.x = x;
    event.pointer.y = y;
    input->emit_event(event);
}

void pointer_handle_button(void* data, struct wl_pointer* /*pointer*/, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    auto* input = static_cast<WaylandInput*>(data);
    Event event;
    event.type = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? EventType::PointerButtonPress : EventType::PointerButtonRelease;
    event.timestamp_ns = static_cast<uint64>(time) * 1000000ULL;
    event.pointer.x = input->pointer_x();
    event.pointer.y = input->pointer_y();
    event.pointer.button = translate_linux_button(button);
    event.pointer.serial = serial;
    input->emit_event(event);
}

void pointer_handle_axis(void* data, struct wl_pointer* /*pointer*/, uint32_t time, uint32_t axis, wl_fixed_t value) {
    auto* input = static_cast<WaylandInput*>(data);
    Event event{};
    event.type = EventType::PointerScroll;
    event.timestamp_ns = static_cast<uint64>(time) * 1000000ULL;
    event.pointer.x = input->pointer_x();
    event.pointer.y = input->pointer_y();
    double delta = wl_fixed_to_double(value);
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        event.pointer.scroll_delta_y = delta;
    } else {
        event.pointer.scroll_delta_x = delta;
    }
    input->emit_event(event);
}

void pointer_handle_frame(void* /*data*/, struct wl_pointer* /*pointer*/) {}
void pointer_handle_axis_source(void* /*data*/, struct wl_pointer* /*pointer*/, uint32_t /*source*/) {}
void pointer_handle_axis_stop(void* /*data*/, struct wl_pointer* /*pointer*/, uint32_t /*time*/, uint32_t /*axis*/) {}
void pointer_handle_axis_discrete(void* /*data*/, struct wl_pointer* /*pointer*/, uint32_t /*axis*/, int32_t /*discrete*/) {}
void pointer_handle_axis_value120(void* /*data*/, struct wl_pointer* /*pointer*/, uint32_t /*axis*/, int32_t /*value120*/) {}
void pointer_handle_axis_relative_direction(void* /*data*/, struct wl_pointer* /*pointer*/, uint32_t /*axis*/, uint32_t /*direction*/) {}

const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_handle_enter,
    .leave = pointer_handle_leave,
    .motion = pointer_handle_motion,
    .button = pointer_handle_button,
    .axis = pointer_handle_axis,
    .frame = pointer_handle_frame,
    .axis_source = pointer_handle_axis_source,
    .axis_stop = pointer_handle_axis_stop,
    .axis_discrete = pointer_handle_axis_discrete,
    .axis_value120 = pointer_handle_axis_value120,
    .axis_relative_direction = pointer_handle_axis_relative_direction
};

void seat_handle_capabilities(void* data, struct wl_seat* /*seat*/, uint32_t capabilities) {
    auto* input = static_cast<WaylandInput*>(data);
    input->bind_capabilities(capabilities);
}

void seat_handle_name(void* /*data*/, struct wl_seat* /*seat*/, const char* /*name*/) {}

const struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name
};

} // namespace

WaylandInput::WaylandInput(wl_seat* seat) noexcept : m_seat(seat) {
    if (m_seat != nullptr) {
        wl_seat_add_listener(m_seat, &seat_listener, this);
    }
}

WaylandInput::WaylandInput(WaylandInput&& other) noexcept
    : m_seat(std::exchange(other.m_seat, nullptr)),
      m_keyboard(std::exchange(other.m_keyboard, nullptr)),
      m_pointer(std::exchange(other.m_pointer, nullptr)),
      m_sink_ctx(std::exchange(other.m_sink_ctx, nullptr)),
      m_sink(std::exchange(other.m_sink, nullptr)),
      m_active_window_id(std::exchange(other.m_active_window_id, 0)),
      m_pointer_x(other.m_pointer_x),
      m_pointer_y(other.m_pointer_y),
      m_active_modifiers(other.m_active_modifiers) {}

WaylandInput& WaylandInput::operator=(WaylandInput&& other) noexcept {
    if (this != &other) {
        if (m_keyboard != nullptr) wl_keyboard_destroy(m_keyboard);
        if (m_pointer != nullptr) wl_pointer_destroy(m_pointer);
        if (m_seat != nullptr) wl_seat_destroy(m_seat);

        m_seat = std::exchange(other.m_seat, nullptr);
        m_keyboard = std::exchange(other.m_keyboard, nullptr);
        m_pointer = std::exchange(other.m_pointer, nullptr);
        m_sink_ctx = std::exchange(other.m_sink_ctx, nullptr);
        m_sink = std::exchange(other.m_sink, nullptr);
        m_active_window_id = std::exchange(other.m_active_window_id, 0);
        m_pointer_x = other.m_pointer_x;
        m_pointer_y = other.m_pointer_y;
        m_active_modifiers = other.m_active_modifiers;
    }
    return *this;
}

WaylandInput::~WaylandInput() noexcept {
    if (m_keyboard != nullptr) wl_keyboard_destroy(m_keyboard);
    if (m_pointer != nullptr) wl_pointer_destroy(m_pointer);
    // Note: m_seat is owned by WaylandConnection; we do not call wl_seat_destroy here.
}

std::unique_ptr<WaylandInput> WaylandInput::create(wl_seat* seat) noexcept {
    if (seat == nullptr) {
        return nullptr;
    }
    return std::unique_ptr<WaylandInput>(new WaylandInput(seat));
}

void WaylandInput::set_event_sink(void* ctx, EventSink sink, uint32 window_id) noexcept {
    m_sink_ctx = ctx;
    m_sink = sink;
    m_active_window_id = window_id;
}

void WaylandInput::emit_event(const Event& raw_event) noexcept {
    if (m_sink != nullptr) {
        Event event = raw_event;
        event.window_id = m_active_window_id;
        if (event.type == EventType::KeyDown || event.type == EventType::KeyUp) {
            event.keyboard.modifiers = m_active_modifiers;
        }
        m_sink(m_sink_ctx, event);
    }
}

void WaylandInput::bind_capabilities(uint32 caps) noexcept {
    bool has_kb = (caps & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
    bool has_ptr = (caps & WL_SEAT_CAPABILITY_POINTER) != 0;

    if (has_kb && m_keyboard == nullptr) {
        m_keyboard = wl_seat_get_keyboard(m_seat);
        if (m_keyboard != nullptr) {
            wl_keyboard_add_listener(m_keyboard, &keyboard_listener, this);
        }
    } else if (!has_kb && m_keyboard != nullptr) {
        wl_keyboard_destroy(m_keyboard);
        m_keyboard = nullptr;
    }

    if (has_ptr && m_pointer == nullptr) {
        m_pointer = wl_seat_get_pointer(m_seat);
        if (m_pointer != nullptr) {
            wl_pointer_add_listener(m_pointer, &pointer_listener, this);
        }
    } else if (!has_ptr && m_pointer != nullptr) {
        wl_pointer_destroy(m_pointer);
        m_pointer = nullptr;
    }
}

void WaylandInput::update_modifiers(uint32 mods_depressed, uint32 /*mods_latched*/, uint32 /*mods_locked*/) noexcept {
    m_active_modifiers = KeyModifier::None;
    if ((mods_depressed & 1U) != 0) m_active_modifiers |= KeyModifier::Shift;
    if ((mods_depressed & 4U) != 0) m_active_modifiers |= KeyModifier::Ctrl;
    if ((mods_depressed & 8U) != 0) m_active_modifiers |= KeyModifier::Alt;
    if ((mods_depressed & 64U) != 0) m_active_modifiers |= KeyModifier::Super;
}

Key translate_linux_keycode(uint32 keycode) noexcept {
    switch (keycode) {
        case KEY_A: return Key::A;
        case KEY_B: return Key::B;
        case KEY_C: return Key::C;
        case KEY_D: return Key::D;
        case KEY_E: return Key::E;
        case KEY_F: return Key::F;
        case KEY_G: return Key::G;
        case KEY_H: return Key::H;
        case KEY_I: return Key::I;
        case KEY_J: return Key::J;
        case KEY_K: return Key::K;
        case KEY_L: return Key::L;
        case KEY_M: return Key::M;
        case KEY_N: return Key::N;
        case KEY_O: return Key::O;
        case KEY_P: return Key::P;
        case KEY_Q: return Key::Q;
        case KEY_R: return Key::R;
        case KEY_S: return Key::S;
        case KEY_T: return Key::T;
        case KEY_U: return Key::U;
        case KEY_V: return Key::V;
        case KEY_W: return Key::W;
        case KEY_X: return Key::X;
        case KEY_Y: return Key::Y;
        case KEY_Z: return Key::Z;
        case KEY_0: return Key::N0;
        case KEY_1: return Key::N1;
        case KEY_2: return Key::N2;
        case KEY_3: return Key::N3;
        case KEY_4: return Key::N4;
        case KEY_5: return Key::N5;
        case KEY_6: return Key::N6;
        case KEY_7: return Key::N7;
        case KEY_8: return Key::N8;
        case KEY_9: return Key::N9;
        case KEY_ESC: return Key::Escape;
        case KEY_ENTER: return Key::Enter;
        case KEY_SPACE: return Key::Space;
        case KEY_BACKSPACE: return Key::Backspace;
        case KEY_TAB: return Key::Tab;
        case KEY_UP: return Key::Up;
        case KEY_DOWN: return Key::Down;
        case KEY_LEFT: return Key::Left;
        case KEY_RIGHT: return Key::Right;
        case KEY_SLASH: return Key::Slash;
        case KEY_DOT: return Key::Period;
        case KEY_MINUS: return Key::Minus;
        case KEY_BACKSLASH: return Key::Backslash;
        case KEY_COMMA: return Key::Comma;
        case KEY_SEMICOLON: return Key::Semicolon;
        case KEY_APOSTROPHE: return Key::Apostrophe;
        case KEY_GRAVE: return Key::Grave;
        case KEY_EQUAL: return Key::Equal;
        case KEY_LEFTBRACE: return Key::LeftBracket;
        case KEY_RIGHTBRACE: return Key::RightBracket;
        case KEY_DELETE: return Key::Delete;
        case KEY_HOME: return Key::Home;
        case KEY_END: return Key::End;
        case KEY_PAGEUP: return Key::PageUp;
        case KEY_PAGEDOWN: return Key::PageDown;
        case KEY_INSERT: return Key::Insert;
        case KEY_F1: return Key::F1;
        case KEY_F2: return Key::F2;
        case KEY_F3: return Key::F3;
        case KEY_F4: return Key::F4;
        case KEY_F5: return Key::F5;
        case KEY_F6: return Key::F6;
        case KEY_F7: return Key::F7;
        case KEY_F8: return Key::F8;
        case KEY_F9: return Key::F9;
        case KEY_F10: return Key::F10;
        case KEY_F11: return Key::F11;
        case KEY_F12: return Key::F12;
        default: return Key::Unknown;
    }
}

MouseButton translate_linux_button(uint32 button) noexcept {
    switch (button) {
        case BTN_LEFT: return MouseButton::Left;
        case BTN_RIGHT: return MouseButton::Right;
        case BTN_MIDDLE: return MouseButton::Middle;
        case BTN_SIDE: return MouseButton::Back;
        case BTN_EXTRA: return MouseButton::Forward;
        default: return MouseButton::None;
    }
}

} // namespace txui::wayland
