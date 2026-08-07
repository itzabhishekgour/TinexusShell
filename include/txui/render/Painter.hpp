#pragma once

#include <txui/render/CommandBuffer.hpp>

namespace txui {

class Painter {
private:
    CommandBuffer& m_buffer;

public:
    explicit Painter(CommandBuffer& buffer) noexcept : m_buffer(buffer) {}

    void begin_frame() {
        m_buffer.push(BeginFrameCommand{});
    }

    void end_frame() {
        m_buffer.push(EndFrameCommand{});
    }

    void set_brush(const Brush& brush) {
        m_buffer.push(SetBrushCommand{brush});
    }

    void fill_rect(const Rect& rect, const Brush& brush) {
        m_buffer.push(DrawRectCommand{rect, brush});
    }

    void fill_rect(const Rect& rect, const Color& color) {
        fill_rect(rect, Brush{SolidBrush{color}});
    }

    void fill_rounded_rect(const Rect& rect, Coordinate radius, const Brush& brush) {
        m_buffer.push(DrawRoundedRectCommand{rect, radius, brush});
    }

    void fill_rounded_rect(const Rect& rect, Coordinate radius, const Color& color) {
        fill_rounded_rect(rect, radius, Brush{SolidBrush{color}});
    }

    // Linear gradient: color_start at top, color_end at bottom (or L->R if horizontal)
    void fill_gradient_rect(const Rect& rect,
                            const Color& color_start, const Color& color_end,
                            bool horizontal = false) {
        m_buffer.push(DrawGradientRectCommand{rect, color_start, color_end, horizontal});
    }

    // Rounded rect with gradient
    void fill_gradient_rounded_rect(const Rect& rect, Coordinate radius,
                                    const Color& color_start, const Color& color_end,
                                    bool horizontal = false) {
        m_buffer.push(DrawGradientRoundedRectCommand{rect, radius, color_start, color_end, horizontal});
    }

    // Filled circle
    void fill_circle(const Point& center, double radius, const Color& color) {
        m_buffer.push(DrawCircleCommand{center, radius, color, 0.0});
    }

    // Circle outline (stroke_width pixels thick)
    void draw_circle(const Point& center, double radius, double stroke_width, const Color& color) {
        m_buffer.push(DrawCircleCommand{center, radius, color, stroke_width});
    }

    // Convenience: glow halo — several concentric semi-transparent circles expanding outward
    void draw_glow(const Point& center, double inner_r, double outer_r, const Color& color) {
        const int steps = 8;
        for (int i = steps; i >= 1; --i) {
            double t    = static_cast<double>(i) / static_cast<double>(steps);
            double r    = inner_r + (outer_r - inner_r) * (1.0 - t);
            uint8  alpha = static_cast<uint8>(static_cast<double>(color.a()) * t * 0.35);
            if (alpha == 0) continue;
            m_buffer.push(DrawCircleCommand{center, r, Color(color.r(), color.g(), color.b(), alpha), 0.0});
        }
    }

    void draw_text(const Point& pos, const std::string& text, const Color& color, double scale = 1.0) {
        m_buffer.push(DrawTextCommand{pos, text, color, scale});
    }
};

} // namespace txui
