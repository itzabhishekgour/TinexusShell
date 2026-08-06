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

    void draw_text(const Point& pos, const std::string& text, const Color& color, double scale = 1.0) {
        m_buffer.push(DrawTextCommand{pos, text, color, scale});
    }
};

} // namespace txui
