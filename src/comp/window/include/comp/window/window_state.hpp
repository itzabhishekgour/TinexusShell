#ifndef TINEXUS_COMP_WINDOW_STATE_HPP
#define TINEXUS_COMP_WINDOW_STATE_HPP

#include <cstdint>
#include <string>

namespace tinexus::comp {

enum class WindowState : uint8_t {
    Normal,
    Maximized,
    Minimized,
    Fullscreen,
    Closing
};

inline const char* to_string(WindowState state) noexcept {
    switch (state) {
        case WindowState::Normal: return "Normal";
        case WindowState::Maximized: return "Maximized";
        case WindowState::Minimized: return "Minimized";
        case WindowState::Fullscreen: return "Fullscreen";
        case WindowState::Closing: return "Closing";
    }
    return "Unknown";
}

struct WindowBox {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};

    bool operator==(const WindowBox& other) const noexcept {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
    bool operator!=(const WindowBox& other) const noexcept {
        return !(*this == other);
    }
};

struct WindowGeometryModel {
    WindowBox normal_geom{50, 100, 800, 600}; // Restoration geometry
    WindowBox current_geom{50, 100, 800, 600};
    void* assigned_output{nullptr};            // Pointer to active output
};

class WindowStateMachine {
public:
    explicit WindowStateMachine(WindowBox initial_geom = {50, 100, 800, 600})
        : m_state(WindowState::Normal), m_geom{initial_geom, initial_geom, nullptr} {}

    [[nodiscard]] WindowState state() const noexcept { return m_state; }
    [[nodiscard]] const WindowGeometryModel& geometry() const noexcept { return m_geom; }
    [[nodiscard]] WindowGeometryModel& geometry() noexcept { return m_geom; }

    bool request_maximize(const WindowBox& target_work_area) {
        if (m_state == WindowState::Closing) return false;
        if (m_state == WindowState::Maximized) return false;

        if (m_state == WindowState::Normal) {
            m_geom.normal_geom = m_geom.current_geom;
        }

        m_state = WindowState::Maximized;
        m_geom.current_geom = target_work_area;
        return true;
    }

    bool request_restore() {
        if (m_state == WindowState::Closing) return false;
        if (m_state != WindowState::Maximized && m_state != WindowState::Minimized && m_state != WindowState::Fullscreen) {
            return false;
        }

        m_state = WindowState::Normal;
        m_geom.current_geom = m_geom.normal_geom;
        return true;
    }

    bool request_minimize() {
        if (m_state == WindowState::Closing) return false;
        if (m_state == WindowState::Minimized) return false;

        if (m_state == WindowState::Normal) {
            m_geom.normal_geom = m_geom.current_geom;
        }
        m_state = WindowState::Minimized;
        return true;
    }

    bool request_fullscreen(const WindowBox& full_output_box) {
        if (m_state == WindowState::Closing) return false;
        if (m_state == WindowState::Fullscreen) return false;

        if (m_state == WindowState::Normal) {
            m_geom.normal_geom = m_geom.current_geom;
        }
        m_state = WindowState::Fullscreen;
        m_geom.current_geom = full_output_box;
        return true;
    }

    void mark_closing() noexcept {
        m_state = WindowState::Closing;
    }

    void update_floating_geometry(int32_t x, int32_t y, int32_t width, int32_t height) noexcept {
        m_geom.current_geom = {x, y, width, height};
        if (m_state == WindowState::Normal) {
            m_geom.normal_geom = m_geom.current_geom;
        }
    }

    void set_assigned_output(void* output) noexcept {
        m_geom.assigned_output = output;
    }

private:
    WindowState m_state{WindowState::Normal};
    WindowGeometryModel m_geom;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WINDOW_STATE_HPP
