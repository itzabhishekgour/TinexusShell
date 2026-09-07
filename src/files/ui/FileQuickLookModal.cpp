#include "files/ui/FileQuickLookModal.hpp"
#include "files/ThumbnailCache.hpp"
#include "files/file_model.hpp"
#include <txui/render/FontMetrics.hpp>
#include <fstream>
#include <algorithm>

namespace tinexus::files::ui {

FileQuickLookModal::FileQuickLookModal() {
}

void FileQuickLookModal::open_file(const std::filesystem::path& path) {
    m_path = path;
    m_lines.clear();

    std::error_code ec;
    if (std::filesystem::exists(path, ec) && !std::filesystem::is_directory(path, ec)) {
        auto item = FileModel::stat_file(path);
        if (item.mime_type.starts_with("text/") || path.extension() == ".txt" ||
            path.extension() == ".cpp" || path.extension() == ".hpp" ||
            path.extension() == ".json" || path.extension() == ".toml" ||
            path.extension() == ".md") {
            std::ifstream file(path);
            std::string line;
            size_t count = 0;
            while (std::getline(file, line) && count < 30) {
                m_lines.push_back(line);
                count++;
            }
        }
    }

    m_is_open = true;
    mark_needs_layout();
    mark_needs_paint();
}

void FileQuickLookModal::close_modal() noexcept {
    if (m_is_open) {
        m_is_open = false;
        mark_needs_paint();
    }
}

txui::Size FileQuickLookModal::measure_override(const txui::Constraints& constraints) noexcept {
    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void FileQuickLookModal::layout_override(const txui::Rect& f) noexcept {
    const double card_w = std::min(640.0, f.width() - 40.0);
    const double card_h = std::min(480.0, f.height() - 40.0);
    const double card_x = f.left() + (f.width() - card_w) * 0.5;
    const double card_y = f.top() + (f.height() - card_h) * 0.5;

    m_card_rect = txui::Rect(card_x, card_y, card_w, card_h);
    m_close_rect = txui::Rect(card_x + card_w - 32.0, card_y + 10.0, 20.0, 20.0);
}

void FileQuickLookModal::paint_override(txui::Painter& painter) const noexcept {
    if (!m_is_open) return;

    // 1. Scrim
    painter.fill_rect(frame(), txui::Color(0, 0, 0, 140));

    // 2. Card Background & Shadow
    painter.fill_rounded_rect(txui::Rect(m_card_rect.left() - 2.0, m_card_rect.top() - 2.0,
                                         m_card_rect.width() + 4.0, m_card_rect.height() + 4.0),
                              12.0, txui::Color(60, 65, 85, 120));
    painter.fill_rounded_rect(m_card_rect, 12.0, txui::Color(28, 30, 42, 255));

    // 3. Header bar
    painter.fill_rounded_rect(txui::Rect(m_card_rect.left(), m_card_rect.top(), m_card_rect.width(), 40.0),
                              12.0, txui::Color(35, 38, 52, 255));
    painter.fill_rect(txui::Rect(m_card_rect.left(), m_card_rect.top() + 39.0, m_card_rect.width(), 1.0),
                      txui::Color(55, 58, 75, 255));

    // 4. Close button (x)
    txui::Color close_bg = m_close_hovered ? txui::Color(75, 80, 105, 255) : txui::Color(55, 60, 80, 255);
    painter.fill_circle(txui::Point(m_close_rect.left() + 10.0, m_close_rect.top() + 10.0), 9.0, close_bg);
    painter.draw_text(txui::Point(m_close_rect.left() + 6.5, m_close_rect.top() + 3.0),
                      "x", txui::Color(220, 225, 240, 255), 11.0);

    // 5. Title
    std::string ql_title = "Quick Look - " + m_path.filename().string();
    auto title_sz = txui::FontMetrics::measure(ql_title, 13.0, true);
    const double max_title_w = m_card_rect.width() - 64.0;
    if (title_sz.width > max_title_w && ql_title.size() > 6) {
        while (ql_title.size() > 1 &&
               txui::FontMetrics::measure(ql_title + "...", 13.0, true).width > max_title_w) {
            ql_title.pop_back();
        }
        ql_title += "...";
    }
    painter.draw_text(txui::Point(m_card_rect.left() + 16.0, m_card_rect.top() + 12.0),
                      ql_title, txui::Color(255, 255, 255, 255), 13.0, true);

    // 6. Body content
    txui::Rect body(m_card_rect.left() + 16.0, m_card_rect.top() + 50.0,
                    m_card_rect.width() - 32.0, m_card_rect.height() - 66.0);

    auto item = FileModel::stat_file(m_path);
    if (item.mime_type.starts_with("image/")) {
        uint32_t max_d = static_cast<uint32_t>(std::min(body.width(), body.height()));
        auto thumb = ThumbnailCache::instance().get_thumbnail(m_path, max_d);
        if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
            double ix = body.left() + (body.width() - thumb->width) * 0.5;
            double iy = body.top() + (body.height() - thumb->height) * 0.5;
            painter.draw_image(txui::Rect(ix, iy, thumb->width, thumb->height),
                               thumb->pixels, thumb->width, thumb->height);
        }
    } else if (!m_lines.empty()) {
        painter.fill_rounded_rect(body, 6.0, txui::Color(20, 22, 32, 255));
        double ty = body.top() + 8.0;
        for (const auto& line : m_lines) {
            if (ty + 16.0 > body.bottom()) break;
            std::string l = line;
            if (l.size() > 75) l = l.substr(0, 72) + "...";
            painter.draw_mono_text(txui::Point(body.left() + 10.0, ty), l,
                                   txui::Color(210, 220, 240, 240), 11.0);
            ty += 16.0;
        }
    } else {
        // Generic preview card
        painter.fill_circle(txui::Point(body.left() + body.width() * 0.5, body.top() + 70.0),
                            30.0, txui::Color(55, 120, 240, 240));
        std::string s_info = "File: " + item.name + " (" + item.mime_type + ")";
        auto s_sz = txui::FontMetrics::measure(s_info, 12.0, false);
        painter.draw_text(txui::Point(body.left() + (body.width() - s_sz.width) * 0.5, body.top() + 120.0),
                          s_info, txui::Color(210, 215, 235, 240), 12.0);
    }
}

bool FileQuickLookModal::handle_event(const txui::Event& event) noexcept {
    if (!m_is_open) return false;

    if (event.type == txui::EventType::KeyDown) {
        if (event.keyboard.key == txui::Key::Space || event.keyboard.key == txui::Key::Escape) {
            close_modal();
            if (on_close) on_close();
            return true;
        }
    }

    if (event.type == txui::EventType::PointerMove) {
        bool h = m_close_rect.contains(event.pointer.x, event.pointer.y);
        if (h != m_close_hovered) {
            m_close_hovered = h;
            mark_needs_paint();
        }
        return true; // Modal captures hover
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        if (m_close_rect.contains(px, py) || !m_card_rect.contains(px, py)) {
            close_modal();
            if (on_close) on_close();
            return true;
        }
        return true; // Click inside card consumed
    }

    return true; // Modal is blocking while open
}

} // namespace tinexus::files::ui
