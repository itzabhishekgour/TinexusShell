#include "files/ui/FileToolbarWidget.hpp"
#include <txui/render/FontMetrics.hpp>
#include <algorithm>
#include <cmath>

namespace tinexus::files::ui {

FileToolbarWidget::FileToolbarWidget() {
}

void FileToolbarWidget::set_history_state(bool can_back, bool can_forward) noexcept {
    if (m_can_back != can_back || m_can_forward != can_forward) {
        m_can_back = can_back;
        m_can_forward = can_forward;
        mark_needs_paint();
    }
}

void FileToolbarWidget::set_view_mode(ViewMode mode) noexcept {
    if (m_view_mode != mode) {
        m_view_mode = mode;
        mark_needs_paint();
    }
}

void FileToolbarWidget::set_sort(SortCriteria criteria, SortDirection direction) noexcept {
    if (m_sort_criteria != criteria || m_sort_direction != direction) {
        m_sort_criteria = criteria;
        m_sort_direction = direction;
        mark_needs_paint();
    }
}

void FileToolbarWidget::set_breadcrumbs(const std::vector<std::pair<std::string, std::filesystem::path>>& crumbs) {
    m_breadcrumbs.clear();
    m_breadcrumbs.reserve(crumbs.size());
    for (const auto& c : crumbs) {
        m_breadcrumbs.push_back({c.first, c.second, txui::Rect{}, false});
    }
    mark_needs_layout();
    mark_needs_paint();
}

void FileToolbarWidget::set_search_query(const std::string& query) {
    if (m_search_query != query) {
        m_search_query = query;
        mark_needs_paint();
    }
}

void FileToolbarWidget::clear_search() {
    if (!m_search_query.empty()) {
        m_search_query.clear();
        m_search_focused = false;
        if (on_search_changed) {
            on_search_changed("");
        }
        mark_needs_paint();
    }
}

txui::Size FileToolbarWidget::measure_override(const txui::Constraints& constraints) noexcept {
    return constraints.constrain(txui::Size(constraints.max_width, 44.0));
}

void FileToolbarWidget::layout_override(const txui::Rect& f) noexcept {
    const double btn_y = f.top() + 8.0;
    const double btn_size = 28.0;

    // 1. Navigation buttons
    m_back_rect = txui::Rect(f.left() + 12.0, btn_y, btn_size, btn_size);
    m_fwd_rect  = txui::Rect(f.left() + 12.0 + btn_size + 6.0, btn_y, btn_size, btn_size);

    // 2. View Mode Switcher
    const double vm_x = m_fwd_rect.right() + 14.0;
    const double seg_w = 28.0;
    m_view_grid_rect = txui::Rect(vm_x, btn_y, seg_w, btn_size);
    m_view_list_rect = txui::Rect(vm_x + seg_w, btn_y, seg_w, btn_size);
    m_view_col_rect  = txui::Rect(vm_x + seg_w * 2.0, btn_y, seg_w, btn_size);
    m_view_gal_rect  = txui::Rect(vm_x + seg_w * 3.0, btn_y, seg_w, btn_size);

    // 3. New Item (+) button
    m_new_btn_rect = txui::Rect(vm_x + seg_w * 4.0 + 8.0, btn_y, btn_size, btn_size);

    // 4. Right side controls
    double right_x = f.right() - 12.0;

    // Search field
    const double search_w = 150.0;
    m_search_rect = txui::Rect(right_x - search_w, btn_y + 1.0, search_w, 26.0);
    m_search_clear_rect = txui::Rect(m_search_rect.right() - 20.0, btn_y + 4.0, 16.0, 18.0);
    right_x -= (search_w + 10.0);

    // Sort button
    const double sort_w = 95.0;
    m_sort_btn_rect = txui::Rect(right_x - sort_w, btn_y + 1.0, sort_w, 26.0);
    right_x -= (sort_w + 12.0);

    // 5. Breadcrumbs
    const double bc_start_x = m_new_btn_rect.right() + 14.0;
    const double bc_max_w = std::max(60.0, right_x - bc_start_x);
    double cur_bc_x = bc_start_x;

    for (auto& bc : m_breadcrumbs) {
        auto text_size = txui::FontMetrics::measure(bc.name, 11.5, false);
        double pill_w = std::max(28.0, text_size.width + 16.0);

        if (cur_bc_x + pill_w > bc_start_x + bc_max_w) {
            bc.rect = txui::Rect{};
            continue;
        }
        bc.rect = txui::Rect(cur_bc_x, btn_y + 2.0, pill_w, 24.0);
        cur_bc_x += pill_w + 14.0;
    }
}

void FileToolbarWidget::paint_override(txui::Painter& painter) const noexcept {
    const double gx = frame().left();
    const double gy = frame().top();
    const double W  = frame().width();
    const double H  = frame().height();

    // 1. Background & hairline divider
    painter.fill_gradient_rect(
        txui::Rect(gx, gy, W, H),
        txui::Color(34, 36, 50, 255),
        txui::Color(28, 30, 42, 255)
    );
    painter.fill_rect(txui::Rect(gx, gy + H - 1.0, W, 1.0), txui::Color(48, 52, 70, 255));

    // 2. Navigation buttons (<, >)
    auto draw_nav_btn = [&](const txui::Rect& r, const std::string& symbol, bool hovered, bool enabled) {
        txui::Color bg = hovered && enabled ? txui::Color(65, 70, 95, 255) : txui::Color(42, 45, 62, 255);
        txui::Color fg = enabled ? txui::Color(230, 235, 250, 255) : txui::Color(100, 105, 125, 180);
        painter.fill_rounded_rect(r, 6.0, bg);
        auto sym_sz = txui::FontMetrics::measure(symbol, 13.0, true);
        double sx = r.left() + (r.width() - sym_sz.width) * 0.5;
        double sy = r.top() + (r.height() - sym_sz.height) * 0.5;
        painter.draw_text(txui::Point(sx, sy), symbol, fg, 13.0, true);
    };

    draw_nav_btn(m_back_rect, "<", m_back_hovered, m_can_back);
    draw_nav_btn(m_fwd_rect,  ">", m_fwd_hovered,  m_can_forward);

    // 3. View Mode Switcher Pill
    txui::Rect vm_full(m_view_grid_rect.left(), m_view_grid_rect.top(), m_view_gal_rect.right() - m_view_grid_rect.left(), 28.0);
    painter.fill_rounded_rect(vm_full, 6.0, txui::Color(36, 38, 54, 255));

    auto draw_mode_seg = [&](const txui::Rect& r, const char* symbol, bool active, bool hovered) {
        if (active) {
            painter.fill_rounded_rect(r, 5.0, txui::Color(55, 125, 245, 255));
            painter.draw_text(txui::Point(r.left() + (r.width() - 10.0)/2.0, r.top() + 6.0), symbol, txui::Color(255, 255, 255, 255), 11.0, true);
        } else {
            if (hovered) painter.fill_rounded_rect(r, 5.0, txui::Color(255, 255, 255, 18));
            painter.draw_text(txui::Point(r.left() + (r.width() - 10.0)/2.0, r.top() + 6.0), symbol, txui::Color(160, 165, 185, 240), 11.0, false);
        }
    };

    draw_mode_seg(m_view_grid_rect, "::", m_view_mode == ViewMode::IconGrid, m_view_grid_hovered);
    draw_mode_seg(m_view_list_rect, "=",  m_view_mode == ViewMode::List,     m_view_list_hovered);
    draw_mode_seg(m_view_col_rect,  "||", m_view_mode == ViewMode::Column,   m_view_col_hovered);
    draw_mode_seg(m_view_gal_rect,  "#",  m_view_mode == ViewMode::Gallery,  m_view_gal_hovered);

    // 4. New Action Button (+)
    txui::Color new_bg = m_new_btn_hovered ? txui::Color(55, 62, 85, 255) : txui::Color(38, 42, 58, 220);
    painter.fill_rounded_rect(m_new_btn_rect, 6.0, new_bg);
    painter.draw_text(txui::Point(m_new_btn_rect.left() + 9.5, m_new_btn_rect.top() + 4.5), "+", txui::Color(240, 245, 255, 255), 14.0, true);

    // 5. Breadcrumbs
    for (size_t i = 0; i < m_breadcrumbs.size(); ++i) {
        const auto& bc = m_breadcrumbs[i];
        if (bc.rect.width() <= 0) continue;
        bool is_last = (i == m_breadcrumbs.size() - 1);
        txui::Color pill_bg = bc.hovered ? txui::Color(60, 65, 90, 255) : (is_last ? txui::Color(45, 50, 72, 200) : txui::Color(35, 38, 54, 160));
        txui::Color text_fg = is_last ? txui::Color(255, 255, 255, 255) : txui::Color(190, 195, 215, 230);

        painter.fill_rounded_rect(bc.rect, 5.0, pill_bg);
        painter.draw_text(txui::Point(bc.rect.left() + 8.0, bc.rect.top() + 5.0), bc.name, text_fg, 11.5, is_last);

        if (!is_last) {
            painter.draw_text(txui::Point(bc.rect.right() + 4.0, bc.rect.top() + 5.0), ">", txui::Color(110, 115, 135, 200), 11.0);
        }
    }

    // 6. Sort Button
    txui::Color sort_bg = m_sort_hovered ? txui::Color(55, 60, 80, 255) : txui::Color(38, 42, 58, 255);
    painter.fill_rounded_rect(m_sort_btn_rect, 6.0, sort_bg);
    std::string sort_text = "Sort: ";
    switch (m_sort_criteria) {
        case SortCriteria::Name:         sort_text += "Name"; break;
        case SortCriteria::DateModified: sort_text += "Date"; break;
        case SortCriteria::Size:         sort_text += "Size"; break;
        case SortCriteria::Kind:         sort_text += "Kind"; break;
    }
    sort_text += (m_sort_direction == SortDirection::Ascending) ? " ^" : " v";
    painter.draw_text(txui::Point(m_sort_btn_rect.left() + 8.0, m_sort_btn_rect.top() + 6.0), sort_text, txui::Color(200, 205, 225, 255), 11.0);

    // 7. Search Field
    txui::Color search_bg = m_search_focused ? txui::Color(45, 50, 70, 255) : txui::Color(35, 38, 52, 255);
    txui::Color search_border = m_search_focused ? txui::Color(60, 130, 250, 255) : txui::Color(55, 60, 80, 255);
    painter.fill_rounded_rect(m_search_rect, 6.0, search_border);
    painter.fill_rounded_rect(txui::Rect(m_search_rect.left() + 1.0, m_search_rect.top() + 1.0, m_search_rect.width() - 2.0, m_search_rect.height() - 2.0), 5.0, search_bg);

    // Magnifying glass icon
    painter.draw_circle(txui::Point(m_search_rect.left() + 12.0, m_search_rect.top() + 12.0), 4.5, 1.2, txui::Color(140, 145, 165, 220));
    painter.draw_line(txui::Point(m_search_rect.left() + 15.5, m_search_rect.top() + 15.5), txui::Point(m_search_rect.left() + 19.0, m_search_rect.top() + 19.0), 1.2, txui::Color(140, 145, 165, 220));

    if (m_search_query.empty()) {
        painter.draw_text(txui::Point(m_search_rect.left() + 24.0, m_search_rect.top() + 6.0), "Search", txui::Color(120, 125, 145, 180), 11.5);
    } else {
        painter.draw_text(txui::Point(m_search_rect.left() + 24.0, m_search_rect.top() + 6.0), m_search_query, txui::Color(240, 245, 255, 255), 11.5, true);
        // Clear 'x' button
        painter.fill_circle(txui::Point(m_search_clear_rect.left() + 8.0, m_search_clear_rect.top() + 9.0), 6.5, txui::Color(65, 70, 90, 255));
        painter.draw_text(txui::Point(m_search_clear_rect.left() + 5.0, m_search_clear_rect.top() + 2.0), "x", txui::Color(200, 205, 225, 255), 10.0);
    }
}

bool FileToolbarWidget::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerMove) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        bool nb_hover = m_back_rect.contains(px, py);
        bool nf_hover = m_fwd_rect.contains(px, py);
        bool nvg_hover = m_view_grid_rect.contains(px, py);
        bool nvl_hover = m_view_list_rect.contains(px, py);
        bool nvc_hover = m_view_col_rect.contains(px, py);
        bool nva_hover = m_view_gal_rect.contains(px, py);
        bool nnew_hover = m_new_btn_rect.contains(px, py);
        bool nsort_hover = m_sort_btn_rect.contains(px, py);
        bool nsearch_hover = m_search_rect.contains(px, py);

        bool changed = (nb_hover != m_back_hovered) || (nf_hover != m_fwd_hovered) ||
                       (nvg_hover != m_view_grid_hovered) || (nvl_hover != m_view_list_hovered) ||
                       (nvc_hover != m_view_col_hovered) || (nva_hover != m_view_gal_hovered) ||
                       (nnew_hover != m_new_btn_hovered) || (nsort_hover != m_sort_hovered) ||
                       (nsearch_hover != m_search_hovered);

        m_back_hovered = nb_hover;
        m_fwd_hovered = nf_hover;
        m_view_grid_hovered = nvg_hover;
        m_view_list_hovered = nvl_hover;
        m_view_col_hovered = nvc_hover;
        m_view_gal_hovered = nva_hover;
        m_new_btn_hovered = nnew_hover;
        m_sort_hovered = nsort_hover;
        m_search_hovered = nsearch_hover;

        for (auto& bc : m_breadcrumbs) {
            bool h = bc.rect.contains(px, py);
            if (h != bc.hovered) {
                bc.hovered = h;
                changed = true;
            }
        }

        if (changed) mark_needs_paint();
        return changed;
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        if (m_back_rect.contains(px, py)) {
            if (m_can_back && on_back) on_back();
            return true;
        }
        if (m_fwd_rect.contains(px, py)) {
            if (m_can_forward && on_forward) on_forward();
            return true;
        }
        if (m_view_grid_rect.contains(px, py)) {
            set_view_mode(ViewMode::IconGrid);
            if (on_view_mode_changed) on_view_mode_changed(ViewMode::IconGrid);
            return true;
        }
        if (m_view_list_rect.contains(px, py)) {
            set_view_mode(ViewMode::List);
            if (on_view_mode_changed) on_view_mode_changed(ViewMode::List);
            return true;
        }
        if (m_view_col_rect.contains(px, py)) {
            set_view_mode(ViewMode::Column);
            if (on_view_mode_changed) on_view_mode_changed(ViewMode::Column);
            return true;
        }
        if (m_view_gal_rect.contains(px, py)) {
            set_view_mode(ViewMode::Gallery);
            if (on_view_mode_changed) on_view_mode_changed(ViewMode::Gallery);
            return true;
        }
        if (m_new_btn_rect.contains(px, py)) {
            if (on_new_item_clicked) on_new_item_clicked();
            return true;
        }
        if (m_sort_btn_rect.contains(px, py)) {
            if (on_sort_clicked) on_sort_clicked();
            return true;
        }

        // Clear search
        if (!m_search_query.empty() && m_search_clear_rect.contains(px, py)) {
            clear_search();
            return true;
        }

        if (m_search_rect.contains(px, py)) {
            m_search_focused = true;
            mark_needs_paint();
            return true;
        } else {
            if (m_search_focused) {
                m_search_focused = false;
                mark_needs_paint();
            }
        }

        for (const auto& bc : m_breadcrumbs) {
            if (bc.rect.contains(px, py)) {
                if (on_breadcrumb_clicked) on_breadcrumb_clicked(bc.path);
                return true;
            }
        }
    }

    if (event.type == txui::EventType::KeyDown && m_search_focused) {
        if (event.keyboard.key == txui::Key::Escape || event.keyboard.key == txui::Key::Enter) {
            m_search_focused = false;
            mark_needs_paint();
            return true;
        }
        if (event.keyboard.key == txui::Key::Backspace) {
            if (!m_search_query.empty()) {
                m_search_query.pop_back();
                if (on_search_changed) on_search_changed(m_search_query);
                mark_needs_paint();
            }
            return true;
        }

        char ch = 0;
        if (event.keyboard.key >= txui::Key::A && event.keyboard.key <= txui::Key::Z) {
            ch = 'a' + static_cast<char>(static_cast<uint16_t>(event.keyboard.key) - static_cast<uint16_t>(txui::Key::A));
        } else if (event.keyboard.key >= txui::Key::N0 && event.keyboard.key <= txui::Key::N9) {
            ch = '0' + static_cast<char>(static_cast<uint16_t>(event.keyboard.key) - static_cast<uint16_t>(txui::Key::N0));
        } else if (event.keyboard.key == txui::Key::Period) {
            ch = '.';
        } else if (event.keyboard.key == txui::Key::Minus) {
            ch = '-';
        } else if (event.keyboard.key == txui::Key::Space) {
            ch = ' ';
        }

        if (ch != 0) {
            m_search_query += ch;
            if (on_search_changed) on_search_changed(m_search_query);
            mark_needs_paint();
            return true;
        }
    }

    return false;
}

} // namespace tinexus::files::ui
