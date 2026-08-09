#pragma once

#include <txui/core/Types.hpp>

namespace txui {

enum class EventType : uint8 {
    None = 0,
    KeyDown,
    KeyUp,
    KeyRepeat,
    PointerEnter,
    PointerLeave,
    PointerMove,
    PointerButtonPress,
    PointerButtonRelease,
    PointerScroll,
    WindowResize,
    WindowClose,
    FrameReady
};

enum class Key : uint16 {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    N0, N1, N2, N3, N4, N5, N6, N7, N8, N9,
    Escape, Enter, Space, Backspace, Tab,
    Up, Down, Left, Right,
    Slash, Period, Minus,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12
};

enum class MouseButton : uint8 {
    None = 0,
    Left = 1,
    Right = 2,
    Middle = 3,
    Back = 4,
    Forward = 5
};

enum class KeyModifier : uint8 {
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
    Super = 1 << 3,
    Caps  = 1 << 4,
    Num   = 1 << 5
};

constexpr KeyModifier operator|(KeyModifier lhs, KeyModifier rhs) noexcept {
    return static_cast<KeyModifier>(static_cast<uint8>(lhs) | static_cast<uint8>(rhs));
}

constexpr KeyModifier operator&(KeyModifier lhs, KeyModifier rhs) noexcept {
    return static_cast<KeyModifier>(static_cast<uint8>(lhs) & static_cast<uint8>(rhs));
}

constexpr KeyModifier& operator|=(KeyModifier& lhs, KeyModifier rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool has_modifier(KeyModifier mask, KeyModifier target) noexcept {
    return (static_cast<uint8>(mask) & static_cast<uint8>(target)) != 0;
}

enum class WindowState : uint8 {
    Creating = 0,
    Running,
    Hidden,
    Minimized,
    Closing,
    Destroyed
};

struct KeyboardEvent {
    Key key;
    KeyModifier modifiers;
    bool is_repeat;
    uint32 serial;
};

struct PointerEvent {
    double x;
    double y;
    MouseButton button;
    double scroll_delta_x;
    double scroll_delta_y;
    uint32 serial;
};

struct ResizeEvent {
    uint32 width;
    uint32 height;
};

struct CloseEvent {
    bool requested;
};

struct Event {
    EventType type;
    uint64 timestamp_ns;
    uint32 window_id;
    union {
        KeyboardEvent keyboard;
        PointerEvent pointer;
        ResizeEvent resize;
        CloseEvent close;
    };
};

} // namespace txui
