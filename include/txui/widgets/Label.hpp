#pragma once

#include <txui/widgets/Widget.hpp>
#include <string>

namespace txui {

class Label : public Widget {
private:
    std::string m_text;
    Color m_color{255, 255, 255, 255};
    double m_font_size{14.0};

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    Label() noexcept = default;
    explicit Label(std::string text) noexcept : m_text(std::move(text)) {}
    ~Label() override = default;

    void set_text(std::string text) noexcept;
    void set_color(Color color) noexcept;
    void set_font_size(double size) noexcept;

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
};

} // namespace txui
