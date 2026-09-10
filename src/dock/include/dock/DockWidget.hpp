#pragma once

#include "txui/widgets/Widget.hpp"
#include "txui/animation/SpringState.hpp"
#include "txui/widgets/Icon.hpp"
#include <string>
#include <vector>

namespace txui {

enum class DockIconAppState {
    NotRunning,
    RunningFocused,
    RunningBg,
    Minimized
};

struct DockIconState {
    std::string app_id;
    std::string label;
    std::string exec;
    txui::IconType icon_type;
    DockIconAppState app_state{DockIconAppState::NotRunning};
    
    txui::SpringState scale_spring;
    txui::SpringState bounce_offset_spring;
    
    double center_x{0.0};
};

class DockWidget : public Widget {
public:
    // ── Design tokens ────────────────────────────────────────────────────────
    static constexpr double BASE_SIZE          = 40.0;
    static constexpr double GAP                = 8.0;
    static constexpr double DOCK_PAD           = 8.0;
    static constexpr double DOCK_BOT_MARGIN    = 8.0;
    static constexpr double PILL_RADIUS        = 18.0;
    static constexpr double ICON_RADIUS        = 10.0;
    static constexpr double MAX_SCALE          = 1.68;
    static constexpr double INFLUENCE_R        = 3.5 * BASE_SIZE;
    static constexpr double INTENTIONAL_SPACING= 8.0;

    [[nodiscard]] static constexpr int32_t pill_height() noexcept {
        return static_cast<int32_t>(BASE_SIZE + DOCK_PAD * 2.0);
    }

    [[nodiscard]] static constexpr int32_t desired_exclusive_zone() noexcept {
        return static_cast<int32_t>(DOCK_BOT_MARGIN + pill_height() + INTENTIONAL_SPACING);
    }

    [[nodiscard]] static constexpr int32_t window_height() noexcept {
        return 120;
    }

    DockWidget();
    ~DockWidget() override;

    void update_icon_state(const std::string& app_id, DockIconAppState state);
    void handle_hover(int x, int y);
    void reset_hover();
    
    bool tick_animations(double dt);
    bool handle_event(const Event& event) noexcept override;

    const std::vector<DockIconState>& icons() const { return m_icons; }

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

private:
    void on_icon_clicked(size_t icon_idx);
    void send_ipc(uint16_t msg_type, const std::string& app_id);
    void spawn_app(const std::string& exec_cmd);

    mutable std::vector<DockIconState> m_icons;
    int m_mouse_x{-1};
    int m_hovered_idx{-1};
    int m_ipc_socket{-1};
    
    void setup_ipc();
};

} // namespace txui
