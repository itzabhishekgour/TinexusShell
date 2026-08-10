#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/layout/ScrollArea.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/widgets/Label.hpp>
#include <functional>
#include "files/column_view_model.hpp"

namespace tinexus::files::ui {

class ColumnBrowserWidget : public txui::Widget {
private:
    txui::Ref<txui::FlexLayout> m_root_layout;
    txui::Ref<txui::ScrollArea> m_scroll_area;
    txui::Ref<txui::FlexLayout> m_columns_layout;
    
    // Preview Panel
    txui::Ref<txui::FlexLayout> m_preview_panel;
    txui::Ref<txui::Label> m_preview_name;
    txui::Ref<txui::Label> m_preview_type;
    txui::Ref<txui::Label> m_preview_size;

    ColumnViewModel m_model;
    std::function<void(const std::filesystem::path&)> m_on_execute;
    std::function<void(const std::filesystem::path&)> m_on_trash;
    std::function<void(const std::filesystem::path&)> m_on_uninstall;

    void on_item_selected(size_t col_index, size_t item_index);
    void on_item_double_clicked(size_t col_index, size_t item_index);

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

public:
    ColumnBrowserWidget();
    ~ColumnBrowserWidget() override = default;

    void navigate_to(const std::filesystem::path& path);
    void set_on_execute(std::function<void(const std::filesystem::path&)> callback) { m_on_execute = std::move(callback); }
    void set_on_trash(std::function<void(const std::filesystem::path&)> callback) { m_on_trash = std::move(callback); }
    void set_on_uninstall(std::function<void(const std::filesystem::path&)> callback) { m_on_uninstall = std::move(callback); }

    [[nodiscard]] std::filesystem::path get_selected_path() const;

    bool handle_event(const txui::Event& event) noexcept override;
};

} // namespace tinexus::files::ui
