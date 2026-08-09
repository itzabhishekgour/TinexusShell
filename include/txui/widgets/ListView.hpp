#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/layout/ScrollArea.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <functional>

namespace txui {

class ListView : public Widget {
private:
    Ref<ScrollArea> m_scroll_area;
    Ref<FlexLayout> m_content_layout;

    int32 m_selected_index{-1};
    int32 m_hover_index{-1};

    std::function<void(int32)> m_on_selected;
    std::function<void(int32)> m_on_double_clicked;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void layout_override(const Rect& frame) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    ListView() noexcept;
    ~ListView() override = default;

    void add_item(Ref<Widget> item) noexcept;
    void clear_items() noexcept;

    void set_on_selected(std::function<void(int32)> callback) { m_on_selected = std::move(callback); }
    void set_on_double_clicked(std::function<void(int32)> callback) { m_on_double_clicked = std::move(callback); }

    [[nodiscard]] int32 selected_index() const noexcept { return m_selected_index; }
    void set_selected_index(int32 index) noexcept;

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
