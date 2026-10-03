#ifndef TINEXUS_COMP_WINDOW_STATE_HPP
#define TINEXUS_COMP_WINDOW_STATE_HPP

#include <cstdint>
#include <string>
#include <algorithm>

namespace tinexus::comp {

enum class WindowState : uint8_t {
    Normal,
    Maximized,
    Minimized,
    Fullscreen,
    Closing
};

enum class SnapMode : uint8_t {
    None,
    Left,
    Right,
    Top,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
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

inline const char* to_string(SnapMode mode) noexcept {
    switch (mode) {
        case SnapMode::None: return "None";
        case SnapMode::Left: return "Left";
        case SnapMode::Right: return "Right";
        case SnapMode::Top: return "Top";
        case SnapMode::TopLeft: return "TopLeft";
        case SnapMode::TopRight: return "TopRight";
        case SnapMode::BottomLeft: return "BottomLeft";
        case SnapMode::BottomRight: return "BottomRight";
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

struct WorkArea {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};

    bool operator==(const WorkArea& other) const noexcept {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
    bool operator!=(const WorkArea& other) const noexcept {
        return !(*this == other);
    }
};

struct OutputGeometry {
    void* output{nullptr};

    int32_t global_x{0};
    int32_t global_y{0};

    int32_t logical_width{0};
    int32_t logical_height{0};

    double scale{1.0};

    int32_t work_x{0};
    int32_t work_y{0};
    int32_t work_width{0};
    int32_t work_height{0};

    uint32_t refresh_mhz{60000};
    std::string connector{};

    // Compatibility aliases for existing callers
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};

    void sync_compat() noexcept {
        x = global_x;
        y = global_y;
        width = logical_width;
        height = logical_height;
    }

    [[nodiscard]] WorkArea work_area() const noexcept {
        return WorkArea{work_x, work_y, work_width, work_height};
    }
    [[nodiscard]] WindowBox full_box() const noexcept {
        return WindowBox{global_x, global_y, logical_width, logical_height};
    }
};

struct WindowGeometryModel {
    WindowBox normal_geom{50, 100, 800, 600}; // Restoration geometry
    WindowBox current_geom{50, 100, 800, 600};
    WindowBox snap_geom{0, 0, 0, 0};
    void* assigned_output{nullptr};            // Pointer to active output
    SnapMode snap_mode{SnapMode::None};
};

class WindowStateMachine {
public:
    explicit WindowStateMachine(WindowBox initial_geom = {50, 100, 800, 600})
        : m_state(WindowState::Normal),
          m_geom{initial_geom, initial_geom, {0, 0, 0, 0}, nullptr, SnapMode::None} {}

    [[nodiscard]] WindowState state() const noexcept { return m_state; }
    [[nodiscard]] SnapMode snap_mode() const noexcept { return m_geom.snap_mode; }
    [[nodiscard]] const WindowGeometryModel& geometry() const noexcept { return m_geom; }
    [[nodiscard]] WindowGeometryModel& geometry() noexcept { return m_geom; }

    static WindowBox compute_snap_box(SnapMode mode, const WorkArea& area) noexcept {
        if (mode == SnapMode::None || area.width <= 0 || area.height <= 0) {
            return {area.x, area.y, area.width, area.height};
        }

        const int32_t half_w = area.width / 2;
        const int32_t rest_w = area.width - half_w; // Exact pixel distribution
        const int32_t half_h = area.height / 2;
        const int32_t rest_h = area.height - half_h;

        switch (mode) {
            case SnapMode::Left:
                return {area.x, area.y, half_w, area.height};
            case SnapMode::Right:
                return {area.x + half_w, area.y, rest_w, area.height};
            case SnapMode::Top:
                return {area.x, area.y, area.width, area.height};
            case SnapMode::TopLeft:
                return {area.x, area.y, half_w, half_h};
            case SnapMode::TopRight:
                return {area.x + half_w, area.y, rest_w, half_h};
            case SnapMode::BottomLeft:
                return {area.x, area.y + half_h, half_w, rest_h};
            case SnapMode::BottomRight:
                return {area.x + half_w, area.y + half_h, rest_w, rest_h};
            case SnapMode::None:
                break;
        }
        return {area.x, area.y, area.width, area.height};
    }

    bool request_maximize(const WorkArea& target_work_area) {
        if (m_state == WindowState::Closing) return false;
        if (m_state == WindowState::Maximized) return false;

        if (m_state == WindowState::Normal && m_geom.snap_mode == SnapMode::None) {
            m_geom.normal_geom = m_geom.current_geom;
        }

        m_state = WindowState::Maximized;
        m_geom.snap_mode = SnapMode::None;
        m_geom.current_geom = {target_work_area.x, target_work_area.y, target_work_area.width, target_work_area.height};
        return true;
    }

    bool request_snap(SnapMode mode, const WorkArea& target_work_area) {
        if (m_state == WindowState::Closing) return false;
        if (mode == SnapMode::None) return false;
        if (mode == SnapMode::Top) {
            return request_maximize(target_work_area);
        }

        if (m_state == WindowState::Normal && m_geom.snap_mode == SnapMode::None) {
            m_geom.normal_geom = m_geom.current_geom;
        }

        m_geom.snap_mode = mode;
        m_geom.snap_geom = compute_snap_box(mode, target_work_area);
        m_geom.current_geom = m_geom.snap_geom;
        m_state = WindowState::Normal; // Snapped is a Normal state with SnapMode active
        return true;
    }

    bool request_restore() {
        if (m_state == WindowState::Closing) return false;
        if (m_state != WindowState::Maximized && m_state != WindowState::Minimized &&
            m_state != WindowState::Fullscreen && m_geom.snap_mode == SnapMode::None) {
            return false;
        }

        m_state = WindowState::Normal;
        m_geom.snap_mode = SnapMode::None;
        m_geom.current_geom = m_geom.normal_geom;
        return true;
    }

    bool request_minimize() {
        if (m_state == WindowState::Closing) return false;
        if (m_state == WindowState::Minimized) return false;

        if (m_state == WindowState::Normal && m_geom.snap_mode == SnapMode::None) {
            m_geom.normal_geom = m_geom.current_geom;
        }
        m_state = WindowState::Minimized;
        return true;
    }

    bool request_fullscreen(const WindowBox& full_output_box) {
        if (m_state == WindowState::Closing) return false;
        if (m_state == WindowState::Fullscreen) return false;

        if (m_state == WindowState::Normal && m_geom.snap_mode == SnapMode::None) {
            m_geom.normal_geom = m_geom.current_geom;
        }
        m_state = WindowState::Fullscreen;
        m_geom.snap_mode = SnapMode::None;
        m_geom.current_geom = full_output_box;
        return true;
    }

    bool request_fullscreen(const OutputGeometry& full_output_box) {
        return request_fullscreen(full_output_box.full_box());
    }

    void mark_closing() noexcept {
        m_state = WindowState::Closing;
    }

    void update_floating_geometry(int32_t x, int32_t y, int32_t width, int32_t height) noexcept {
        m_geom.current_geom = {x, y, width, height};
        if (m_state == WindowState::Normal && m_geom.snap_mode == SnapMode::None) {
            m_geom.normal_geom = m_geom.current_geom;
        }
    }

    void set_assigned_output(void* output) noexcept {
        m_geom.assigned_output = output;
    }

    void update_work_area(const WorkArea& wa) noexcept {
        if (m_state == WindowState::Maximized) {
            m_geom.current_geom = {wa.x, wa.y, wa.width, wa.height};
        }
    }

private:
    WindowState m_state{WindowState::Normal};
    WindowGeometryModel m_geom;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WINDOW_STATE_HPP
