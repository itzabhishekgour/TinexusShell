#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/math/Insets.hpp>

namespace txui {

class Padding final : public Widget {
private:
    Insets m_padding;

protected:
    Size measure_override(const Constraints& constraints) noexcept override {
        if (children().empty()) {
            return Size(m_padding.horizontal(), m_padding.vertical());
        }

        auto child = children().front();
        float64 horiz = m_padding.horizontal();
        float64 vert = m_padding.vertical();

        Constraints child_constraints(
            std::max(0.0, constraints.min_width - horiz),
            std::max(0.0, constraints.max_width - horiz),
            std::max(0.0, constraints.min_height - vert),
            std::max(0.0, constraints.max_height - vert)
        );

        child->measure(child_constraints);
        
        return Size(
            child->desired_size().width + horiz,
            child->desired_size().height + vert
        );
    }

    void layout_override(const Rect& frame) noexcept override {
        if (!children().empty()) {
            auto child = children().front();
            child->layout(Rect(
                frame.left() + m_padding.left,
                frame.top() + m_padding.top,
                child->desired_size().width,
                child->desired_size().height
            ));
        }
    }

    void paint_override(Painter& painter) const noexcept override {
        if (!children().empty()) {
            children().front()->paint(painter);
        }
    }

public:
    explicit Padding(const Insets& padding, Ref<Widget> child = nullptr) noexcept 
        : m_padding(padding) {
        if (child) {
            add_child(std::move(child));
        }
    }

    ~Padding() override = default;

    [[nodiscard]] const Insets& padding() const noexcept { return m_padding; }
    
    void set_padding(const Insets& padding) noexcept {
        if (m_padding.left != padding.left || m_padding.top != padding.top ||
            m_padding.right != padding.right || m_padding.bottom != padding.bottom) {
            m_padding = padding;
            mark_needs_measure();
        }
    }
};

} // namespace txui
