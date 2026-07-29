#pragma once

#include <txui/core/Object.hpp>
#include <txui/core/Ref.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Size.hpp>
#include <txui/layout/Constraints.hpp>
#include <txui/render/Painter.hpp>
#include <vector>

namespace txui {

class Widget : public Object {
private:
    Widget* m_parent{nullptr}; // Raw pointer to avoid cycles
    std::vector<Ref<Widget>> m_children;

    Size m_desired_size{0.0, 0.0};
    Rect m_frame{0.0, 0.0, 0.0, 0.0};

    bool m_needs_measure{true};
    bool m_needs_layout{true};
    bool m_needs_paint{true};

protected:
    // Core layout pipeline overrides
    virtual Size measure_override(const Constraints& constraints) noexcept = 0;
    virtual void layout_override(const Rect& frame) noexcept;
    virtual void paint_override(Painter& painter) const noexcept = 0;

public:
    Widget() noexcept = default;
    ~Widget() override = default;

    // Hierarchy management
    [[nodiscard]] Widget* parent() const noexcept { return m_parent; }
    [[nodiscard]] const std::vector<Ref<Widget>>& children() const noexcept { return m_children; }
    
    void add_child(Ref<Widget> child) noexcept;
    void remove_child(Widget* child) noexcept;
    void remove_all_children() noexcept;

    // Geometry access (immutable by user, mutated only by pipeline)
    [[nodiscard]] Size desired_size() const noexcept { return m_desired_size; }
    [[nodiscard]] Rect frame() const noexcept { return m_frame; }

    // Dirty flags API
    [[nodiscard]] bool needs_measure() const noexcept { return m_needs_measure; }
    [[nodiscard]] bool needs_layout() const noexcept { return m_needs_layout; }
    [[nodiscard]] bool needs_paint() const noexcept { return m_needs_paint; }

    void mark_needs_measure() noexcept;
    void mark_needs_layout() noexcept;
    void mark_needs_paint() noexcept;

    // Three-Phase Layout Pipeline
    void measure(const Constraints& constraints) noexcept;
    void layout(const Rect& frame) noexcept;
    void paint(Painter& painter) const noexcept;

protected:
    void paint_children(Painter& painter) const noexcept;
};

} // namespace txui
