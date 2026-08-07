#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/core/Ref.hpp>

namespace txui {

// ─────────────────────────────────────────────────────────────────────────────
// Padding — wraps a single child with uniform or per-edge insets
// ─────────────────────────────────────────────────────────────────────────────
struct EdgeInsets {
    float64 top{0.0}, right{0.0}, bottom{0.0}, left{0.0};

    constexpr EdgeInsets() noexcept = default;
    // All sides uniform
    constexpr explicit EdgeInsets(float64 all) noexcept
        : top(all), right(all), bottom(all), left(all) {}
    // Symmetric
    constexpr EdgeInsets(float64 vertical, float64 horizontal) noexcept
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    // Per-edge
    constexpr EdgeInsets(float64 t, float64 r, float64 b, float64 l) noexcept
        : top(t), right(r), bottom(b), left(l) {}

    [[nodiscard]] constexpr float64 horizontal() const noexcept { return left + right; }
    [[nodiscard]] constexpr float64 vertical()   const noexcept { return top + bottom; }
};

class Padding : public Widget {
public:
    EdgeInsets insets;

    Padding() = default;
    explicit Padding(EdgeInsets e) noexcept : insets(e) {}

    void set_child(Ref<Widget> child) noexcept {
        remove_all_children();
        add_child(std::move(child));
    }

protected:
    Size measure_override(const Constraints& constraints) noexcept override {
        const float64 h_pad = insets.horizontal();
        const float64 v_pad = insets.vertical();

        Constraints child_c{
            std::max(0.0, constraints.min_width  - h_pad),
            std::max(0.0, constraints.max_width  - h_pad),
            std::max(0.0, constraints.min_height - v_pad),
            std::max(0.0, constraints.max_height - v_pad)
        };

        const auto& kids = children();
        if (!kids.empty()) {
            kids[0]->measure(child_c);
            const Size s = kids[0]->desired_size();
            return constraints.constrain(Size(s.width + h_pad, s.height + v_pad));
        }
        return constraints.constrain(Size(h_pad, v_pad));
    }

    void layout_override(const Rect& f) noexcept override {
        const auto& kids = children();
        if (!kids.empty()) {
            kids[0]->layout(Rect(
                f.x() + insets.left,
                f.y() + insets.top,
                f.width()  - insets.horizontal(),
                f.height() - insets.vertical()
            ));
        }
    }

    void paint_override(Painter& painter) const noexcept override {
        paint_children(painter);
    }
};

} // namespace txui
