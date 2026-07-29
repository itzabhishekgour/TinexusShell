#pragma once

#include <txui/widgets/Widget.hpp>

namespace txui {

class SizedBox final : public Widget {
private:
    float64 m_width;
    float64 m_height;

protected:
    Size measure_override(const Constraints& /*constraints*/) noexcept override {
        Constraints child_constraints = Constraints::tight(m_width, m_height);
        for (auto& child : children()) {
            child->measure(child_constraints);
        }
        return Size(m_width, m_height);
    }

    void paint_override(Painter& painter) const noexcept override {
        paint_children(painter);
    }

public:
    SizedBox(float64 width, float64 height) noexcept 
        : m_width(width), m_height(height) {}
        
    ~SizedBox() override = default;

    void set_size(float64 width, float64 height) noexcept {
        if (m_width != width || m_height != height) {
            m_width = width;
            m_height = height;
            mark_needs_measure();
        }
    }
};

} // namespace txui
