#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/core/Ref.hpp>

namespace txui {

// ─────────────────────────────────────────────────────────────────────────────
// VBox — vertical stack layout widget
//
// Places child widgets one below the other. Each child gets its natural height.
// Width is stretched to fill the available max_width.
// ─────────────────────────────────────────────────────────────────────────────
class VBox : public Widget {
public:
    float64 spacing{0.0}; // Gap in pixels between children

    VBox() = default;
    explicit VBox(float64 spacing_px) noexcept : spacing(spacing_px) {}

    // Convenience: add a child and return *this for chaining
    VBox& add(Ref<Widget> child) noexcept {
        add_child(std::move(child));
        return *this;
    }

protected:
    Size measure_override(const Constraints& constraints) noexcept override {
        const auto& kids = children();
        float64 total_height = 0.0;
        float64 max_child_w  = 0.0;

        // Child constraints: loosen height so children report natural size
        Constraints child_c = constraints.loosen();

        for (size_t i = 0; i < kids.size(); ++i) {
            kids[i]->measure(child_c);
            const Size s = kids[i]->desired_size();
            total_height += s.height;
            if (i + 1 < kids.size()) total_height += spacing;
            if (s.width > max_child_w) max_child_w = s.width;
        }

        return constraints.constrain(Size(
            constraints.is_bounded_width() ? constraints.max_width : max_child_w,
            total_height
        ));
    }

    void layout_override(const Rect& f) noexcept override {
        const auto& kids = children();
        float64 y = f.y();
        for (const auto& child : kids) {
            float64 h = child->desired_size().height;
            child->layout(Rect(f.x(), y, f.width(), h));
            y += h + spacing;
        }
    }

    void paint_override(Painter& painter) const noexcept override {
        paint_children(painter);
    }
};

} // namespace txui
