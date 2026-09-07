#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/Icon.hpp>
#include <string>
#include <optional>
#include <functional>

namespace txui {

class NavItem : public Widget {
public:
    using CustomIconRenderer = std::function<void(Painter&, const Rect&)>;

private:
    std::string m_label;
    std::optional<IconType> m_icon_type;
    CustomIconRenderer m_custom_icon_renderer;
    bool m_selected{false};
    bool m_hovered{false};
    bool m_pressed{false};
    bool m_enabled{true};

    Color m_accent_color{0, 195, 255, 255};
    double m_font_size{14.0};
    double m_corner_radius{8.0};

    std::function<void()> m_on_click;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    NavItem() noexcept = default;
    explicit NavItem(std::string label, std::function<void()> on_click = nullptr) noexcept;
    NavItem(std::string label, IconType icon, std::function<void()> on_click = nullptr) noexcept;
    NavItem(std::string label, CustomIconRenderer custom_icon, std::function<void()> on_click = nullptr) noexcept;
    ~NavItem() override = default;

    void set_label(std::string label) noexcept;
    [[nodiscard]] const std::string& label() const noexcept { return m_label; }

    void set_icon(std::optional<IconType> icon) noexcept;
    [[nodiscard]] std::optional<IconType> icon() const noexcept { return m_icon_type; }

    void set_custom_icon_renderer(CustomIconRenderer renderer) noexcept;

    void set_selected(bool selected) noexcept;
    [[nodiscard]] bool is_selected() const noexcept { return m_selected; }

    void set_accent_color(Color color) noexcept;
    [[nodiscard]] Color accent_color() const noexcept { return m_accent_color; }

    void set_font_size(double size) noexcept;
    [[nodiscard]] double font_size() const noexcept { return m_font_size; }

    void set_corner_radius(double radius) noexcept;
    [[nodiscard]] double corner_radius() const noexcept { return m_corner_radius; }

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool is_enabled() const noexcept { return m_enabled; }

    void set_on_click(std::function<void()> callback) noexcept { m_on_click = std::move(callback); }

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
