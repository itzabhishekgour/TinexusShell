#include "files/ui/FileGalleryView.hpp"
#include "files/ThumbnailCache.hpp"
#include <txui/render/FontMetrics.hpp>
#include <algorithm>
#include <ctime>

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

FileGalleryView::FileGalleryView() {
}

void FileGalleryView::set_items(const std::vector<FileItem>& items) {
    m_items = items;
    if (m_selected_idx >= static_cast<int32_t>(m_items.size())) {
        m_selected_idx = m_items.empty() ? -1 : 0;
    }
    mark_needs_layout();
    mark_needs_paint();
}

void FileGalleryView::set_selected_index(int32_t idx) noexcept {
    if (m_selected_idx != idx) {
        m_selected_idx = idx;
        mark_needs_paint();
    }
}

void FileGalleryView::select_path(const std::filesystem::path& path) {
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].path == path) {
            set_selected_index(static_cast<int32_t>(i));
            return;
        }
    }
}

txui::Size FileGalleryView::measure_override(const txui::Constraints& constraints) noexcept {
    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void FileGalleryView::layout_override(const txui::Rect& /*frame*/) noexcept {
}

void FileGalleryView::paint_override(txui::Painter& painter) const noexcept {
    m_strip_hits.clear();
    const txui::Rect area = frame();

    const double top_h = area.height() * 0.65;
    const double bottom_h = area.height() - top_h;

    txui::Rect top_area(area.left(), area.top(), area.width(), top_h);
    txui::Rect bottom_area(area.left(), area.top() + top_h, area.width(), bottom_h);

    // Divider line between preview and filmstrip
    painter.fill_rect(txui::Rect(area.left(), top_area.bottom(), area.width(), 1.0),
                      txui::Color(45, 48, 65, 255));

    // 1. Large Top Preview
    if (m_selected_idx >= 0 && static_cast<size_t>(m_selected_idx) < m_items.size()) {
        const auto& sel_item = m_items[static_cast<size_t>(m_selected_idx)];
        bool is_image = sel_item.mime_type.starts_with("image/");

        if (is_image) {
            uint32_t target_dim = static_cast<uint32_t>(std::min(top_h - 60.0, top_area.width() - 80.0));
            auto thumb = ThumbnailCache::instance().get_thumbnail(sel_item.path, target_dim);
            if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
                double px = top_area.left() + (top_area.width() - thumb->width) * 0.5;
                double py = top_area.top() + (top_h - 40.0 - thumb->height) * 0.5;
                painter.fill_rounded_rect(txui::Rect(px - 4.0, py - 4.0, thumb->width + 8.0, thumb->height + 8.0),
                                          8.0, txui::Color(16, 18, 25, 255));
                painter.draw_image(txui::Rect(px, py, thumb->width, thumb->height),
                                   thumb->pixels, thumb->width, thumb->height);
            }
        } else {
            // Large Card
            const double card_w = 320.0;
            const double card_h = 160.0;
            const double card_x = top_area.left() + (top_area.width() - card_w) * 0.5;
            const double card_y = top_area.top() + (top_h - card_h) * 0.5 - 10.0;
            txui::Rect card(card_x, card_y, card_w, card_h);
            painter.fill_rounded_rect(card, 10.0, txui::Color(32, 35, 48, 255));

            painter.fill_circle(txui::Point(card.left() + 45.0, card.top() + 45.0), 20.0, txui::Color(55, 120, 240, 240));
            painter.draw_text(txui::Point(card.left() + 80.0, card.top() + 35.0), sel_item.name, txui::Color(255, 255, 255, 255), 14.0, true);
            painter.draw_text(txui::Point(card.left() + 80.0, card.top() + 55.0), format_size(sel_item.size_bytes), txui::Color(150, 155, 175, 220), 12.0);
            painter.draw_text(txui::Point(card.left() + 25.0, card.top() + 90.0), "Type: " + sel_item.mime_type, txui::Color(180, 185, 205, 220), 11.5);
            painter.draw_text(txui::Point(card.left() + 25.0, card.top() + 115.0), "Modified: " + format_time(sel_item.last_modified), txui::Color(180, 185, 205, 220), 11.5);
        }

        // Caption with FontMetrics measure
        std::string cap = sel_item.name + " (" + format_size(sel_item.size_bytes) + ")";
        auto cap_sz = txui::FontMetrics::measure(cap, 12.0, true);
        painter.draw_text(txui::Point(top_area.left() + (top_area.width() - cap_sz.width) * 0.5, top_area.bottom() - 25.0),
                          cap, txui::Color(255, 255, 255, 255), 12.0, true);
    }

    // 2. Bottom Filmstrip
    const double strip_pad_y = 12.0;
    const double strip_cell_w = 70.0;
    const double strip_cell_h = bottom_h - 24.0;
    double cur_x = bottom_area.left() + 16.0;

    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        txui::Rect cell(cur_x, bottom_area.top() + strip_pad_y, strip_cell_w, strip_cell_h);
        m_strip_hits.push_back({i, cell, item.path});

        bool is_selected = (static_cast<int32_t>(i) == m_selected_idx);
        if (is_selected) {
            painter.fill_rounded_rect(cell, 6.0, txui::Color(55, 125, 245, 180));
        } else {
            painter.fill_rounded_rect(cell, 6.0, txui::Color(32, 35, 48, 160));
        }

        // Mini preview inside cell
        txui::Rect mini(cell.left() + 5.0, cell.top() + 5.0, cell.width() - 10.0, cell.height() - 24.0);
        if (item.mime_type.starts_with("image/")) {
            auto thumb = ThumbnailCache::instance().get_thumbnail(item.path, 50);
            if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
                double mx = mini.left() + (mini.width() - thumb->width) * 0.5;
                double my = mini.top() + (mini.height() - thumb->height) * 0.5;
                painter.draw_image(txui::Rect(mx, my, thumb->width, thumb->height),
                                   thumb->pixels, thumb->width, thumb->height);
            }
        } else {
            painter.fill_rounded_rect(txui::Rect(mini.left() + (mini.width() - 24.0) * 0.5, mini.top() + 8.0, 24.0, 24.0),
                                      4.0, txui::Color(55, 120, 240, 220));
        }

        // Mini label with FontMetrics
        std::string mini_name = item.name;
        auto mini_sz = txui::FontMetrics::measure(mini_name, 10.0, false);
        const double max_mini_w = cell.width() - 10.0;
        if (mini_sz.width > max_mini_w && mini_name.size() > 4) {
            while (mini_name.size() > 1 &&
                   txui::FontMetrics::measure(mini_name + "..", 10.0, false).width > max_mini_w) {
                mini_name.pop_back();
            }
            mini_name += "..";
            mini_sz = txui::FontMetrics::measure(mini_name, 10.0, false);
        }
        double mini_x = cell.left() + (cell.width() - mini_sz.width) * 0.5;
        painter.draw_text(txui::Point(mini_x, cell.bottom() - 16.0), mini_name, txui::Color(220, 225, 240, 255), 10.0);

        cur_x += strip_cell_w + 10.0;
    }
}

bool FileGalleryView::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerButtonPress) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        for (const auto& hit : m_strip_hits) {
            if (hit.cell_rect.contains(px, py)) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
                bool is_double = (m_last_clicked_idx == static_cast<int32_t>(hit.index) && elapsed < 400);

                m_last_click_time = now;
                m_last_clicked_idx = static_cast<int32_t>(hit.index);
                set_selected_index(static_cast<int32_t>(hit.index));

                if (event.pointer.button == txui::MouseButton::Right) {
                    if (on_context_menu) on_context_menu(px, py, hit.path);
                    return true;
                }

                if (is_double && on_item_double_clicked) {
                    on_item_double_clicked(static_cast<int32_t>(hit.index), m_items[hit.index]);
                } else if (on_item_selected) {
                    on_item_selected(static_cast<int32_t>(hit.index), m_items[hit.index]);
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
