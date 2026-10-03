#pragma once

#include <txui/widgets/Widget.hpp>
#include <functional>

namespace txui {

class ToggleSwitch : public Widget {
private:
    bool m_checked{false};
    bool m_hovered{false};
    bool m_enabled{true};

    Color m_active_color{0, 195, 255, 255};
    Color m_inactive_color{48, 52, 66, 240};

    std::function<void(bool)> m_on_toggled;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    ToggleSwitch() noexcept = default;
    explicit ToggleSwitch(bool checked, std::function<void(bool)> on_toggled = nullptr) noexcept;
    ~ToggleSwitch() override = default;

    void set_checked(bool checked) noexcept;
    [[nodiscard]] bool is_checked() const noexcept { return m_checked; }

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool is_enabled() const noexcept { return m_enabled; }

    void set_active_color(Color color) noexcept;
    [[nodiscard]] Color active_color() const noexcept { return m_active_color; }

    void set_inactive_color(Color color) noexcept;
    [[nodiscard]] Color inactive_color() const noexcept { return m_inactive_color; }

    void set_on_toggled(std::function<void(bool)> callback) noexcept { m_on_toggled = std::move(callback); }

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
