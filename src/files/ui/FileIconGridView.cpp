#include "files/ui/FileIconGridView.hpp"
#include "files/TagManager.hpp"
#include "files/ThumbnailCache.hpp"
#include <txui/render/FontMetrics.hpp>
#include <algorithm>
#include <cmath>

namespace tinexus::files::ui {

FileIconGridView::FileIconGridView() {
}

void FileIconGridView::set_items(const std::vector<FileItem>& items) {
    m_items = items;
    m_scroll_y = 0.0;
    if (m_selected_idx >= static_cast<int32_t>(m_items.size())) {
        m_selected_idx = m_items.empty() ? -1 : 0;
    }
    mark_needs_layout();
    mark_needs_paint();
}

void FileIconGridView::set_selected_index(int32_t idx) noexcept {
    if (m_selected_idx != idx) {
        m_selected_idx = idx;
        mark_needs_paint();
    }
}

void FileIconGridView::select_path(const std::filesystem::path& path) {
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].path == path) {
            set_selected_index(static_cast<int32_t>(i));
            return;
        }
    }
}

txui::Size FileIconGridView::measure_override(const txui::Constraints& constraints) noexcept {
    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void FileIconGridView::layout_override(const txui::Rect& f) noexcept {
    const double cell_w = 106.0;
    const double cell_h = 112.0;
    const double pad_x = 16.0;
    const double pad_y = 16.0;

    int cols = std::max(1, static_cast<int>((f.width() - pad_x) / (cell_w + pad_x)));
    int rows = static_cast<int>((m_items.size() + static_cast<size_t>(cols) - 1) / static_cast<size_t>(cols));
    double total_h = pad_y * 2.0 + rows * (cell_h + pad_y);

    m_max_scroll_y = std::max(0.0, total_h - f.height());
    m_scroll_y = std::clamp(m_scroll_y, 0.0, m_max_scroll_y);
}

void FileIconGridView::paint_override(txui::Painter& painter) const noexcept {
    m_cells.clear();
    const txui::Rect area = frame();

    if (m_items.empty()) {
        painter.draw_text(txui::Point(area.left() + 30.0, area.top() + 40.0),
                          "Folder is empty", txui::Color(130, 135, 155, 200), 13.0);
        return;
    }

    const double cell_w = 106.0;
    const double cell_h = 112.0;
    const double pad_x = 16.0;
    const double pad_y = 16.0;

    int cols = std::max(1, static_cast<int>((area.width() - pad_x) / (cell_w + pad_x)));
    double start_x = area.left() + pad_x;
    double start_y = area.top() + pad_y - m_scroll_y;

    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        int row = static_cast<int>(i) / cols;
        int col = static_cast<int>(i) % cols;

        double cx = start_x + col * (cell_w + pad_x);
        double cy = start_y + row * (cell_h + pad_y);
        txui::Rect cell(cx, cy, cell_w, cell_h);
        txui::Rect thumb_box(cx + (cell_w - 72.0) * 0.5, cy + 6.0, 72.0, 68.0);

        m_cells.push_back({i, cell, thumb_box, item.path});

        // Visible cull
        if (cell.bottom() < area.top() || cell.top() > area.bottom()) continue;

        bool is_selected = (static_cast<int32_t>(i) == m_selected_idx);
        if (is_selected) {
            painter.fill_rounded_rect(cell, 8.0, txui::Color(55, 120, 240, 70));
            painter.fill_rounded_rect(txui::Rect(cell.left() + 1.0, cell.top() + 1.0, cell.width() - 2.0, cell.height() - 2.0),
                                      7.0, txui::Color(55, 120, 240, 120));
        }

        bool drew_image = false;
        if (item.mime_type.starts_with("image/")) {
            auto thumb = ThumbnailCache::instance().get_thumbnail(item.path, 72);
            if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
                painter.fill_rounded_rect(thumb_box, 6.0, txui::Color(20, 22, 30, 255));
                double tx = thumb_box.left() + (thumb_box.width() - thumb->width) * 0.5;
                double ty = thumb_box.top() + (thumb_box.height() - thumb->height) * 0.5;
                painter.draw_image(txui::Rect(tx, ty, thumb->width, thumb->height), thumb->pixels, thumb->width, thumb->height);
                drew_image = true;
            }
        }

        if (!drew_image) {
            if (item.type == FileType::Directory) {
                // Folder icon
                painter.fill_rounded_rect(thumb_box, 6.0, txui::Color(45, 130, 240, 230));
                painter.fill_rounded_rect(txui::Rect(thumb_box.left() + 6.0, thumb_box.top() + 4.0, 26.0, 8.0), 3.0, txui::Color(70, 160, 255, 255));
                painter.fill_rounded_rect(txui::Rect(thumb_box.left() + 4.0, thumb_box.top() + 10.0, thumb_box.width() - 8.0, thumb_box.height() - 14.0), 5.0, txui::Color(35, 115, 225, 255));
            } else if (item.is_executable) {
                // Executable binary icon
                painter.fill_rounded_rect(thumb_box, 10.0, txui::Color(45, 55, 75, 240));
                painter.fill_circle(txui::Point(thumb_box.left() + thumb_box.width() * 0.5, thumb_box.top() + thumb_box.height() * 0.5), 18.0, txui::Color(255, 135, 45, 240));
                painter.draw_text(txui::Point(thumb_box.left() + thumb_box.width() * 0.5 - 5.0, thumb_box.top() + thumb_box.height() * 0.5 - 7.0), "!", txui::Color(255, 255, 255, 255), 14.0, true);
            } else if (item.mime_type == "application/archive") {
                // Archive icon
                painter.fill_rounded_rect(thumb_box, 6.0, txui::Color(180, 110, 45, 240));
                painter.fill_rect(txui::Rect(thumb_box.left() + thumb_box.width() * 0.5 - 3.0, thumb_box.top() + 6.0, 6.0, thumb_box.height() - 12.0), txui::Color(245, 185, 45, 240));
            } else {
                // Document file
                painter.fill_rounded_rect(thumb_box, 6.0, txui::Color(48, 52, 70, 220));
                painter.fill_rect(txui::Rect(thumb_box.left() + 14.0, thumb_box.top() + 18.0, thumb_box.width() - 28.0, 2.5), txui::Color(160, 165, 185, 200));
                painter.fill_rect(txui::Rect(thumb_box.left() + 14.0, thumb_box.top() + 26.0, thumb_box.width() - 28.0, 2.5), txui::Color(160, 165, 185, 200));
                painter.fill_rect(txui::Rect(thumb_box.left() + 14.0, thumb_box.top() + 34.0, thumb_box.width() - 36.0, 2.5), txui::Color(160, 165, 185, 200));
            }
        }

        // Tag dot if tagged
        auto tag = TagManager::instance().get_tag(item.path);
        if (tag.has_value()) {
            txui::Color tc = TagManager::tag_color(tag.value());
            painter.fill_circle(txui::Point(thumb_box.right() - 4.0, thumb_box.top() + 4.0), 4.5, tc);
        }

        // Exact font measured label with ellipsis
        std::string label_text = item.name;
        const double max_label_w = cell_w - 8.0;
        auto text_size = txui::FontMetrics::measure(label_text, 11.5, is_selected);

        if (text_size.width > max_label_w && label_text.size() > 4) {
            while (label_text.size() > 1 &&
                   txui::FontMetrics::measure(label_text + "...", 11.5, is_selected).width > max_label_w) {
                label_text.pop_back();
            }
            label_text += "...";
            text_size = txui::FontMetrics::measure(label_text, 11.5, is_selected);
        }

        txui::Color text_col = is_selected ? txui::Color(255, 255, 255, 255) : txui::Color(215, 220, 235, 240);
        double text_x = cx + (cell_w - text_size.width) * 0.5;
        painter.draw_text(txui::Point(text_x, cy + 82.0), label_text, text_col, 11.5, is_selected);
    }
}

bool FileIconGridView::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerScroll) {
        m_scroll_y -= event.pointer.scroll_delta_y * 24.0;
        m_scroll_y = std::clamp(m_scroll_y, 0.0, m_max_scroll_y);
        mark_needs_paint();
        return true;
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        for (const auto& cell : m_cells) {
            if (cell.cell_rect.contains(px, py)) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
                bool is_double = (m_last_clicked_idx == static_cast<int32_t>(cell.index) && elapsed < 400);

                m_last_click_time = now;
                m_last_clicked_idx = static_cast<int32_t>(cell.index);
                set_selected_index(static_cast<int32_t>(cell.index));

                if (event.pointer.button == txui::MouseButton::Right) {
                    if (on_context_menu) {
                        on_context_menu(px, py, cell.path);
                    }
                    return true;
                }

                if (is_double && on_item_double_clicked) {
                    on_item_double_clicked(static_cast<int32_t>(cell.index), m_items[cell.index]);
                } else if (on_item_selected) {
                    on_item_selected(static_cast<int32_t>(cell.index), m_items[cell.index]);
                }
                return true;
            }
        }

        // Click on background
        if (frame().contains(px, py)) {
            if (event.pointer.button == txui::MouseButton::Right) {
                if (on_context_menu) {
                    on_context_menu(px, py, "");
                }
                return true;
            }
        }
    }

    return false;
}

} // namespace tinexus::files::ui
