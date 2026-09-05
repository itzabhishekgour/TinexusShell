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

    void clear(const Color& color) {
        m_buffer.push(ClearCommand{color});
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

    void draw_text(const Point& pos, const std::string& text, const Color& color,
                   double scale = 1.0, bool bold = false, bool italic = false,
                   FontFamily font_family = FontFamily::UI) {
        m_buffer.push(DrawTextCommand{pos, text, color, scale, bold, italic, font_family});
    }

    // Convenience overload for terminal and code surfaces — always uses monospace face
    void draw_mono_text(const Point& pos, const std::string& text, const Color& color,
                        double scale = 1.0, bool bold = false) {
        m_buffer.push(DrawTextCommand{pos, text, color, scale, bold, false, FontFamily::Monospace});
    }

    void draw_line(const Point& p1, const Point& p2, double thickness, const Color& color) {
        m_buffer.push(DrawLineCommand{p1, p2, thickness, color});
    }

    void draw_image(const Rect& rect, const std::shared_ptr<std::vector<uint32_t>>& pixels, uint32_t width, uint32_t height) {
        if (pixels && width > 0 && height > 0) {
            m_buffer.push(DrawImageCommand{rect, pixels, width, height});
        }
    }

    void push_clip(const Rect& rect) {
        m_buffer.push(PushClipCommand{rect});
    }

    void pop_clip() {
        m_buffer.push(PopClipCommand{});
    }

    void push_transform(double offset_x, double offset_y) {
        m_buffer.push(PushTransformCommand{offset_x, offset_y});
    }

    void pop_transform() {
        m_buffer.push(PopTransformCommand{});
    }
};

} // namespace txui
