#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/layout/Constraints.hpp>

namespace txui {

enum class FlexDirection {
    Row,
    Column
};

enum class MainAxisAlignment {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

enum class CrossAxisAlignment {
    Start,
    Center,
    End,
    Stretch
};

// Controls how a child widget behaves inside a FlexLayout
class FlexItem final : public Widget {
private:
    int32 m_flex{1};
    bool m_expanded{false};

protected:
    Size measure_override(const Constraints& constraints) noexcept override {
        if (children().empty()) return Size(0.0, 0.0);
        auto child = children().front();
        child->measure(constraints);
        return m_expanded ? constraints.constrain(child->desired_size()) : child->desired_size();
    }

    void layout_override(const Rect& frame) noexcept override {
        if (!children().empty()) {
            children().front()->layout(frame);
        }
    }

    void paint_override(Painter& painter) const noexcept override {
        paint_children(painter);
    }

public:
    explicit FlexItem(int32 flex, bool expanded, Ref<Widget> child = nullptr) noexcept
        : m_flex(std::max(0, flex)), m_expanded(expanded) {
        if (child) add_child(std::move(child));
    }

    ~FlexItem() override = default;

    [[nodiscard]] int32 flex() const noexcept { return m_flex; }
    [[nodiscard]] bool expanded() const noexcept { return m_expanded; }

    static Ref<FlexItem> Expanded(Ref<Widget> child, int32 flex = 1) {
        return make_ref<FlexItem>(flex, true, std::move(child));
    }

    static Ref<FlexItem> Flexible(Ref<Widget> child, int32 flex = 1) {
        return make_ref<FlexItem>(flex, false, std::move(child));
    }
};

class FlexLayout : public Widget {
private:
    FlexDirection m_direction{FlexDirection::Row};
    MainAxisAlignment m_main_axis_alignment{MainAxisAlignment::Start};
    CrossAxisAlignment m_cross_axis_alignment{CrossAxisAlignment::Start};

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void layout_override(const Rect& frame) noexcept override;
    void paint_override(Painter& painter) const noexcept override {
        paint_children(painter);
    }

public:
    FlexLayout() noexcept = default;
    ~FlexLayout() override = default;

    void set_direction(FlexDirection dir) noexcept {
        if (m_direction != dir) {
            m_direction = dir;
            mark_needs_measure();
        }
    }

    void set_main_axis_alignment(MainAxisAlignment align) noexcept {
        if (m_main_axis_alignment != align) {
            m_main_axis_alignment = align;
            mark_needs_layout();
        }
    }

    void set_cross_axis_alignment(CrossAxisAlignment align) noexcept {
        if (m_cross_axis_alignment != align) {
            m_cross_axis_alignment = align;
            mark_needs_layout();
        }
    }
    
    [[nodiscard]] FlexDirection direction() const noexcept { return m_direction; }
    [[nodiscard]] MainAxisAlignment main_axis_alignment() const noexcept { return m_main_axis_alignment; }
    [[nodiscard]] CrossAxisAlignment cross_axis_alignment() const noexcept { return m_cross_axis_alignment; }
};

class Row final : public FlexLayout {
public:
    Row() noexcept { set_direction(FlexDirection::Row); }
};

class Column final : public FlexLayout {
public:
    Column() noexcept { set_direction(FlexDirection::Column); }
};

} // namespace txui
