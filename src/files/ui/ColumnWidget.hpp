#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/ListView.hpp>
#include <txui/layout/FlexLayout.hpp>
#include "files/column_view_model.hpp"

namespace tinexus::files::ui {

class ColumnWidget : public txui::Widget {
private:
    txui::Ref<txui::ListView> m_list_view;
    const ColumnLevel* m_model{nullptr};
    size_t m_col_index{0};

    std::function<void(size_t, size_t)> m_on_item_selected;
    std::function<void(size_t, size_t)> m_on_item_double_clicked;

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

public:
    ColumnWidget(const ColumnLevel& level, size_t col_index);
    ~ColumnWidget() override = default;

    void set_on_item_selected(std::function<void(size_t, size_t)> callback) { m_on_item_selected = std::move(callback); }
    void set_on_item_double_clicked(std::function<void(size_t, size_t)> callback) { m_on_item_double_clicked = std::move(callback); }

    [[nodiscard]] size_t col_index() const noexcept { return m_col_index; }
    void refresh(const ColumnLevel& level) noexcept;

    bool handle_event(const txui::Event& event) noexcept override;
};

} // namespace tinexus::files::ui
