#include "files/ui/FileListView.hpp"
#include "files/TagManager.hpp"
#include <txui/render/FontMetrics.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace tinexus::files::ui {

static std::string format_size(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        return std::string(buf);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return std::string(buf);
}

static std::string format_time(std::filesystem::file_time_type ftime) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm_buf{};
    localtime_r(&cftime, &tm_buf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%b %d, %H:%M", &tm_buf);
    return std::string(buf);
}

FileListView::FileListView() {
}

void FileListView::set_items(const std::vector<FileItem>& items) {
    m_items = items;
    m_scroll_y = 0.0;
    if (m_selected_idx >= static_cast<int32_t>(m_items.size())) {
        m_selected_idx = m_items.empty() ? -1 : 0;
    }
    mark_needs_layout();
    mark_needs_paint();
}

void FileListView::set_sort(SortCriteria criteria, SortDirection direction) noexcept {
    if (m_sort_criteria != criteria || m_sort_direction != direction) {
        m_sort_criteria = criteria;
        m_sort_direction = direction;
        mark_needs_paint();
    }
}

void FileListView::set_selected_index(int32_t idx) noexcept {
    if (m_selected_idx != idx) {
        m_selected_idx = idx;
        mark_needs_paint();
    }
}

void FileListView::select_path(const std::filesystem::path& path) {
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].path == path) {
            set_selected_index(static_cast<int32_t>(i));
            return;
        }
    }
}

txui::Size FileListView::measure_override(const txui::Constraints& constraints) noexcept {
    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void FileListView::layout_override(const txui::Rect& f) noexcept {
    const double name_w = f.width() * 0.45;
    const double date_w = 140.0;
    const double size_w = 85.0;
    const double kind_w = std::max(60.0, f.width() - name_w - date_w - size_w);

    m_hdr_name_rect = txui::Rect(f.left(), f.top(), name_w, 28.0);
    m_hdr_date_rect = txui::Rect(f.left() + name_w, f.top(), date_w, 28.0);
    m_hdr_size_rect = txui::Rect(f.left() + name_w + date_w, f.top(), size_w, 28.0);
    m_hdr_kind_rect = txui::Rect(f.left() + name_w + date_w + size_w, f.top(), kind_w, 28.0);

    const double row_h = 28.0;
    double total_h = 28.0 + static_cast<double>(m_items.size()) * row_h;
    m_max_scroll_y = std::max(0.0, total_h - f.height());
    m_scroll_y = std::clamp(m_scroll_y, 0.0, m_max_scroll_y);
}

void FileListView::paint_override(txui::Painter& painter) const noexcept {
    m_rows.clear();
    const txui::Rect area = frame();

    // 1. Column Headers (height 28px)
    txui::Rect header_bar(area.left(), area.top(), area.width(), 28.0);
    painter.fill_rect(header_bar, txui::Color(32, 35, 48, 255));
    painter.fill_rect(txui::Rect(header_bar.left(), header_bar.bottom() - 1.0, header_bar.width(), 1.0),
                      txui::Color(48, 52, 72, 255));

    auto draw_header_col = [&](const txui::Rect& r, const char* title, SortCriteria crit) {
        bool is_active = (m_sort_criteria == crit);
        std::string t = title;
        if (is_active) {
            t += (m_sort_direction == SortDirection::Ascending) ? " ^" : " v";
        }
        txui::Color fg = is_active ? txui::Color(255, 255, 255, 255) : txui::Color(140, 145, 168, 220);
        painter.draw_text(txui::Point(r.left() + 10.0, header_bar.top() + 7.0), t, fg, 11.0, is_active);
        painter.fill_rect(txui::Rect(r.right() - 1.0, header_bar.top() + 4.0, 1.0, 20.0),
                          txui::Color(45, 48, 64, 200));
    };

    draw_header_col(m_hdr_name_rect, "Name", SortCriteria::Name);
    draw_header_col(m_hdr_date_rect, "Date Modified", SortCriteria::DateModified);
    draw_header_col(m_hdr_size_rect, "Size", SortCriteria::Size);
    draw_header_col(m_hdr_kind_rect, "Kind", SortCriteria::Kind);

    // 2. Table Rows (height 28px)
    const double row_h = 28.0;
    double cur_y = header_bar.bottom() - m_scroll_y;

    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        txui::Rect row_rect(area.left(), cur_y, area.width(), row_h);
        m_rows.push_back({i, row_rect, item.path});

        // Visible cull
        if (row_rect.bottom() < header_bar.bottom() || row_rect.top() > area.bottom()) {
            cur_y += row_h;
            continue;
        }

        bool is_selected = (static_cast<int32_t>(i) == m_selected_idx);
        if (is_selected) {
            painter.fill_rect(row_rect, txui::Color(45, 110, 225, 220));
        } else if (i % 2 == 1) {
            painter.fill_rect(row_rect, txui::Color(255, 255, 255, 6));
        }

        // Icon miniature
        double icon_cx = row_rect.left() + 18.0;
        double icon_cy = row_rect.top() + row_h * 0.5;
        txui::Color icon_col = is_selected ? txui::Color(255, 255, 255, 255) : txui::Color(120, 160, 245, 240);

        if (item.type == FileType::Directory) {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 6.0, icon_cy - 5.0, 12.0, 10.0), 2.0, icon_col);
        } else if (item.mime_type.starts_with("image/")) {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 6.0, icon_cy - 6.0, 12.0, 12.0), 2.0, txui::Color(40, 180, 185, 255));
        } else if (item.is_executable) {
            painter.fill_circle(txui::Point(icon_cx, icon_cy), 5.5, txui::Color(255, 140, 45, 255));
        } else {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 5.0, icon_cy - 6.0, 10.0, 12.0), 1.5, txui::Color(160, 165, 185, 220));
        }

        // Tag dot if tagged
        auto tag = TagManager::instance().get_tag(item.path);
        if (tag.has_value()) {
            painter.fill_circle(txui::Point(row_rect.left() + 32.0, icon_cy), 3.5, TagManager::tag_color(tag.value()));
        }

        // Name column with FontMetrics measure
        double name_x = row_rect.left() + (tag.has_value() ? 42.0 : 34.0);
        double max_name_w = m_hdr_name_rect.width() - (name_x - row_rect.left()) - 10.0;
        std::string disp_name = item.name;
        auto name_sz = txui::FontMetrics::measure(disp_name, 12.0, is_selected);
        if (name_sz.width > max_name_w && disp_name.size() > 4) {
            while (disp_name.size() > 1 &&
                   txui::FontMetrics::measure(disp_name + "...", 12.0, is_selected).width > max_name_w) {
                disp_name.pop_back();
            }
            disp_name += "...";
        }
        txui::Color text_fg = is_selected ? txui::Color(255, 255, 255, 255) : txui::Color(225, 230, 245, 240);
        painter.draw_text(txui::Point(name_x, row_rect.top() + 7.0), disp_name, text_fg, 12.0, is_selected);

        // Date Modified
        txui::Color sec_fg = is_selected ? txui::Color(240, 245, 255, 220) : txui::Color(140, 145, 165, 200);
        painter.draw_text(txui::Point(m_hdr_date_rect.left() + 10.0, row_rect.top() + 7.0),
                          format_time(item.last_modified), sec_fg, 11.5);

        // Size
        std::string size_str = (item.type == FileType::Directory) ? "--" : format_size(item.size_bytes);
        painter.draw_text(txui::Point(m_hdr_size_rect.left() + 10.0, row_rect.top() + 7.0),
                          size_str, sec_fg, 11.5);

        // Kind
        std::string kind_str = (item.type == FileType::Directory) ? "Folder" : item.mime_type;
        double max_kind_w = m_hdr_kind_rect.width() - 20.0;
        auto kind_sz = txui::FontMetrics::measure(kind_str, 11.5, false);
        if (kind_sz.width > max_kind_w && kind_str.size() > 4) {
            while (kind_str.size() > 1 &&
                   txui::FontMetrics::measure(kind_str + "..", 11.5, false).width > max_kind_w) {
                kind_str.pop_back();
            }
            kind_str += "..";
        }
        painter.draw_text(txui::Point(m_hdr_kind_rect.left() + 10.0, row_rect.top() + 7.0),
                          kind_str, sec_fg, 11.5);

        cur_y += row_h;
    }
}

bool FileListView::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerScroll) {
        m_scroll_y -= event.pointer.scroll_delta_y * 24.0;
        m_scroll_y = std::clamp(m_scroll_y, 0.0, m_max_scroll_y);
        mark_needs_paint();
        return true;
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        // Header clicks for sorting
        if (m_hdr_name_rect.contains(px, py)) {
            if (on_sort_changed) on_sort_changed(SortCriteria::Name);
            return true;
        }
        if (m_hdr_date_rect.contains(px, py)) {
            if (on_sort_changed) on_sort_changed(SortCriteria::DateModified);
            return true;
        }
        if (m_hdr_size_rect.contains(px, py)) {
            if (on_sort_changed) on_sort_changed(SortCriteria::Size);
            return true;
        }
        if (m_hdr_kind_rect.contains(px, py)) {
            if (on_sort_changed) on_sort_changed(SortCriteria::Kind);
            return true;
        }

        // Row clicks
        for (const auto& row : m_rows) {
            if (row.row_rect.contains(px, py)) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
                bool is_double = (m_last_clicked_idx == static_cast<int32_t>(row.index) && elapsed < 400);

                m_last_click_time = now;
                m_last_clicked_idx = static_cast<int32_t>(row.index);
                set_selected_index(static_cast<int32_t>(row.index));

                if (event.pointer.button == txui::MouseButton::Right) {
                    if (on_context_menu) on_context_menu(px, py, row.path);
                    return true;
                }

                if (is_double && on_item_double_clicked) {
                    on_item_double_clicked(static_cast<int32_t>(row.index), m_items[row.index]);
                } else if (on_item_selected) {
                    on_item_selected(static_cast<int32_t>(row.index), m_items[row.index]);
                }
                return true;
            }
        }

        // Background right-click
        if (frame().contains(px, py)) {
            if (event.pointer.button == txui::MouseButton::Right) {
                if (on_context_menu) on_context_menu(px, py, "");
                return true;
            }
        }
    }

    return false;
}

} // namespace tinexus::files::ui
