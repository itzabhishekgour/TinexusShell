#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/Icon.hpp>
#include <txui/render/FontMetrics.hpp>
#include <string>
#include <optional>
#include <functional>

namespace txui {

class Button : public Widget {
public:
    enum class Style {
        Standard,
        Primary,
        Danger,
        Ghost
    };

private:
    std::string m_text;
    std::optional<IconType> m_icon;
    Style m_style{Style::Standard};
    double m_font_size{14.0};
    double m_corner_radius{8.0};
    bool m_enabled{true};

    bool m_hovered{false};
    bool m_pressed{false};

    std::function<void()> m_on_click;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    Button() noexcept = default;
    explicit Button(std::string text, std::function<void()> on_click = nullptr) noexcept;
    Button(std::string text, IconType icon, std::function<void()> on_click = nullptr) noexcept;
    ~Button() override = default;

    void set_text(std::string text) noexcept;
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

    void set_icon(std::optional<IconType> icon) noexcept;
    [[nodiscard]] std::optional<IconType> icon() const noexcept { return m_icon; }

    void set_style(Style style) noexcept;
    [[nodiscard]] Style style() const noexcept { return m_style; }

    void set_corner_radius(double radius) noexcept;
    [[nodiscard]] double corner_radius() const noexcept { return m_corner_radius; }

    void set_font_size(double size) noexcept;
    [[nodiscard]] double font_size() const noexcept { return m_font_size; }

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool is_enabled() const noexcept { return m_enabled; }

    void set_on_click(std::function<void()> callback) noexcept { m_on_click = std::move(callback); }

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
