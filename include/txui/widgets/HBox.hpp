#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/core/Ref.hpp>

namespace txui {

// ─────────────────────────────────────────────────────────────────────────────
// HBox — horizontal stack layout widget
//
// Places child widgets side-by-side left→right. Each child gets its natural width.
// Height is stretched to fill available max_height.
// ─────────────────────────────────────────────────────────────────────────────
class HBox : public Widget {
public:
    float64 spacing{0.0}; // Gap in pixels between children

    HBox() = default;
    explicit HBox(float64 spacing_px) noexcept : spacing(spacing_px) {}

    // Convenience: add a child and return *this for chaining
    HBox& add(Ref<Widget> child) noexcept {
        add_child(std::move(child));
        return *this;
    }

protected:
    Size measure_override(const Constraints& constraints) noexcept override {
        const auto& kids = children();
        float64 total_width  = 0.0;
        float64 max_child_h  = 0.0;

        Constraints child_c = constraints.loosen();

        for (size_t i = 0; i < kids.size(); ++i) {
            kids[i]->measure(child_c);
            const Size s = kids[i]->desired_size();
            total_width += s.width;
            if (i + 1 < kids.size()) total_width += spacing;
            if (s.height > max_child_h) max_child_h = s.height;
        }

        return constraints.constrain(Size(
            total_width,
            constraints.is_bounded_height() ? constraints.max_height : max_child_h
        ));
    }

    void layout_override(const Rect& f) noexcept override {
        const auto& kids = children();
        float64 x = f.x();
        for (const auto& child : kids) {
            float64 w = child->desired_size().width;
            child->layout(Rect(x, f.y(), w, f.height()));
            x += w + spacing;
        }
    }

    void paint_override(Painter& painter) const noexcept override {
        paint_children(painter);
    }
};

} // namespace txui
