#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/FontMetrics.hpp>
#include <string>
#include <functional>

namespace txui {

class TextInput : public Widget {
private:
    std::string m_text;
    std::string m_placeholder;
    double m_font_size{14.0};
    double m_corner_radius{8.0};

    size_t m_caret_pos{0};
    size_t m_sel_start{0};
    size_t m_sel_end{0};

    bool m_focused{false};
    bool m_hovered{false};
    bool m_caret_visible{true};
    bool m_secure_mode{false};
    bool m_error{false};
    bool m_enabled{true};

    std::function<void(std::string_view)> m_on_submit;
    std::function<void(std::string_view)> m_on_text_changed;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    TextInput() noexcept = default;
    explicit TextInput(std::string placeholder) noexcept;
    ~TextInput() override = default;

    void set_text(std::string text) noexcept;
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

    void set_placeholder(std::string placeholder) noexcept;
    [[nodiscard]] const std::string& placeholder() const noexcept { return m_placeholder; }

    void set_font_size(double size) noexcept;
    [[nodiscard]] double font_size() const noexcept { return m_font_size; }

    void set_focused(bool focused) noexcept;
    [[nodiscard]] bool is_focused() const noexcept { return m_focused; }

    void set_caret_position(size_t pos) noexcept;
    [[nodiscard]] size_t caret_position() const noexcept { return m_caret_pos; }

    void select_all() noexcept;
    void clear_selection() noexcept;

    void set_on_submit(std::function<void(std::string_view)> callback) noexcept { m_on_submit = std::move(callback); }
    void set_on_text_changed(std::function<void(std::string_view)> callback) noexcept { m_on_text_changed = std::move(callback); }

    void set_secure_mode(bool secure) noexcept;
    [[nodiscard]] bool is_secure_mode() const noexcept { return m_secure_mode; }

    void set_corner_radius(double radius) noexcept;
    [[nodiscard]] double corner_radius() const noexcept { return m_corner_radius; }

    void set_error(bool error) noexcept;
    [[nodiscard]] bool has_error() const noexcept { return m_error; }

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool is_enabled() const noexcept { return m_enabled; }

    bool handle_event(const Event& event) noexcept override;

private:
    [[nodiscard]] std::string display_text() const noexcept;
    [[nodiscard]] std::string display_substring(size_t count) const noexcept;
    size_t char_index_at_x(double local_x) const noexcept;
};

} // namespace txui
