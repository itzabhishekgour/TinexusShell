#include "files/ui/FileInspectorWidget.hpp"
#include "files/ThumbnailCache.hpp"
#include "files/file_model.hpp"
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

FileInspectorWidget::FileInspectorWidget() {
}

void FileInspectorWidget::inspect_path(const std::filesystem::path& path) {
    m_path = path;
    if (m_path.empty()) {
        m_disp_name = "No selection";
        m_is_dir = false;
        m_is_image = false;
        m_kind_str = "--";
        m_size_str = "--";
        m_modified_str = "--";
        mark_needs_paint();
        return;
    }

    std::error_code ec;
    m_is_dir = std::filesystem::is_directory(path, ec);
    std::string ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    m_is_image = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".svg");

    m_disp_name = path.filename().string();
    if (m_disp_name.empty()) m_disp_name = path.string();

    if (m_is_dir) {
        m_kind_str = "Folder";
        auto items = FileModel::scan_directory(path, false);
        m_size_str = std::to_string(items.size()) + " items";
    } else {
        m_kind_str = m_is_image ? ("Image (" + ext + ")") : "Document";
        auto sz = std::filesystem::file_size(path, ec);
        m_size_str = ec ? "--" : format_size(sz);
    }

    auto lwt = std::filesystem::last_write_time(path, ec);
    m_modified_str = ec ? "--" : format_time(lwt);

    mark_needs_layout();
    mark_needs_paint();
}

txui::Size FileInspectorWidget::measure_override(const txui::Constraints& constraints) noexcept {
    return constraints.constrain(txui::Size(230.0, constraints.max_height));
}

void FileInspectorWidget::layout_override(const txui::Rect& f) noexcept {
    const double p_pad = 16.0;
    const double p_w = f.width() - p_pad * 2.0;
    const double p_h = 130.0;

    m_preview_box = txui::Rect(f.left() + p_pad, f.top() + 16.0, p_w, p_h);
    m_open_btn_rect  = txui::Rect(f.left() + 20.0, f.bottom() - 75.0, f.width() - 40.0, 30.0);
    m_trash_btn_rect = txui::Rect(f.left() + 20.0, f.bottom() - 38.0, f.width() - 40.0, 26.0);
}

void FileInspectorWidget::paint_override(txui::Painter& painter) const noexcept {
    const txui::Rect f = frame();

    // 1. Background & separator
    painter.fill_rect(f, txui::Color(20, 22, 32, 255));
    painter.fill_rect(txui::Rect(f.left(), f.top(), 1.0, f.height()), txui::Color(45, 48, 64, 255));

    if (m_path.empty()) {
        painter.draw_text(txui::Point(f.left() + 24.0, f.top() + 40.0),
                          "No selection", txui::Color(120, 125, 145, 200), 13.0);
        return;
    }

    // 2. Preview Box
    painter.fill_rounded_rect(m_preview_box, 8.0, txui::Color(14, 16, 24, 255));
    painter.fill_rounded_rect(txui::Rect(m_preview_box.left() + 1.0, m_preview_box.top() + 1.0,
                                         m_preview_box.width() - 2.0, m_preview_box.height() - 2.0),
                              7.0, txui::Color(26, 28, 40, 255));

    if (m_is_image) {
        auto thumb = ThumbnailCache::instance().get_thumbnail(m_path, 120);
        if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
            double tx = m_preview_box.left() + (m_preview_box.width() - thumb->width) * 0.5;
            double ty = m_preview_box.top() + (m_preview_box.height() - thumb->height) * 0.5;
            painter.draw_image(txui::Rect(tx, ty, thumb->width, thumb->height),
                               thumb->pixels, thumb->width, thumb->height);
        }
    } else if (m_is_dir) {
        double fcx = m_preview_box.left() + m_preview_box.width() * 0.5;
        double fcy = m_preview_box.top() + m_preview_box.height() * 0.5;
        painter.fill_rounded_rect(txui::Rect(fcx - 28.0, fcy - 18.0, 56.0, 38.0), 5.0, txui::Color(45, 130, 240, 240));
        painter.fill_rounded_rect(txui::Rect(fcx - 28.0, fcy - 22.0, 24.0, 8.0), 3.0, txui::Color(70, 160, 255, 255));
    } else {
        double fcx = m_preview_box.left() + m_preview_box.width() * 0.5;
        double fcy = m_preview_box.top() + m_preview_box.height() * 0.5;
        painter.fill_rounded_rect(txui::Rect(fcx - 20.0, fcy - 26.0, 40.0, 52.0), 4.0, txui::Color(55, 60, 80, 240));
        painter.fill_rect(txui::Rect(fcx - 12.0, fcy - 10.0, 24.0, 2.5), txui::Color(160, 165, 185, 200));
        painter.fill_rect(txui::Rect(fcx - 12.0, fcy - 2.0, 24.0, 2.5), txui::Color(160, 165, 185, 200));
        painter.fill_rect(txui::Rect(fcx - 12.0, fcy + 6.0, 16.0, 2.5), txui::Color(160, 165, 185, 200));
    }

    // 3. Name & Divider with FontMetrics
    const double p_pad = 16.0;
    std::string name_str = m_disp_name;
    const double max_name_w = f.width() - p_pad * 2.0;
    auto name_sz = txui::FontMetrics::measure(name_str, 13.0, true);
    if (name_sz.width > max_name_w && name_str.size() > 4) {
        while (name_str.size() > 1 &&
               txui::FontMetrics::measure(name_str + "...", 13.0, true).width > max_name_w) {
            name_str.pop_back();
        }
        name_str += "...";
    }

    double cur_y = m_preview_box.bottom() + 14.0;
    painter.draw_text(txui::Point(f.left() + p_pad, cur_y), name_str, txui::Color(255, 255, 255, 255), 13.0, true);

    cur_y += 22.0;
    painter.fill_rect(txui::Rect(f.left() + p_pad, cur_y, f.width() - p_pad * 2.0, 1.0),
                      txui::Color(45, 48, 65, 200));

    // 4. Metadata details
    cur_y += 10.0;
    auto draw_field = [&](const char* title, const std::string& val) {
        painter.draw_text(txui::Point(f.left() + p_pad, cur_y), title, txui::Color(120, 125, 145, 220), 10.0, true);
        std::string v = val;
        auto val_sz = txui::FontMetrics::measure(v, 11.5, false);
        if (val_sz.width > max_name_w && v.size() > 4) {
            while (v.size() > 1 &&
                   txui::FontMetrics::measure(v + "..", 11.5, false).width > max_name_w) {
                v.pop_back();
            }
            v += "..";
        }
        painter.draw_text(txui::Point(f.left() + p_pad, cur_y + 13.0), v, txui::Color(215, 220, 235, 240), 11.5);
        cur_y += 32.0;
    };

    draw_field("KIND", m_kind_str);
    draw_field("SIZE", m_size_str);
    draw_field("MODIFIED", m_modified_str);

    // 5. Action buttons
    txui::Color open_bg = m_open_hovered ? txui::Color(65, 135, 255, 255) : txui::Color(45, 115, 240, 255);
    painter.fill_rounded_rect(m_open_btn_rect, 6.0, open_bg);
    auto open_txt_sz = txui::FontMetrics::measure("Open", 13.0, true);
    double ox = m_open_btn_rect.left() + (m_open_btn_rect.width() - open_txt_sz.width) * 0.5;
    double oy = m_open_btn_rect.top() + (m_open_btn_rect.height() - open_txt_sz.height) * 0.5;
    painter.draw_text(txui::Point(ox, oy), "Open", txui::Color(255, 255, 255, 255), 13.0, true);

    if (!m_is_dir) {
        txui::Color trash_bg = m_trash_hovered ? txui::Color(80, 35, 45, 255) : txui::Color(45, 25, 32, 255);
        painter.fill_rounded_rect(m_trash_btn_rect, 6.0, trash_bg);
        auto trash_txt_sz = txui::FontMetrics::measure("Move to Trash", 11.5, false);
        double tx = m_trash_btn_rect.left() + (m_trash_btn_rect.width() - trash_txt_sz.width) * 0.5;
        double ty = m_trash_btn_rect.top() + (m_trash_btn_rect.height() - trash_txt_sz.height) * 0.5;
        painter.draw_text(txui::Point(tx, ty), "Move to Trash", txui::Color(245, 120, 120, 230), 11.5);
    }
}

bool FileInspectorWidget::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerMove) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        bool noh = m_open_btn_rect.contains(px, py);
        bool nth = m_trash_btn_rect.contains(px, py);

        if (noh != m_open_hovered || nth != m_trash_hovered) {
            m_open_hovered = noh;
            m_trash_hovered = nth;
            mark_needs_paint();
            return true;
        }
        return false;
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        if (m_open_btn_rect.contains(px, py)) {
            if (on_open_requested) on_open_requested(m_path);
            return true;
        }
        if (!m_is_dir && m_trash_btn_rect.contains(px, py)) {
            if (on_trash_requested) on_trash_requested(m_path);
            return true;
        }
    }

    return false;
}

} // namespace tinexus::files::ui
