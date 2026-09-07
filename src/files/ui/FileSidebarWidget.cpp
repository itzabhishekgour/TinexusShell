#include "files/ui/FileSidebarWidget.hpp"
#include <txui/render/FontMetrics.hpp>
#include <cstdlib>
#include <algorithm>

namespace tinexus::files::ui {

static std::string get_home_dir() {
    const char* home = std::getenv("HOME");
    if (home && *home) return std::string(home);
    return "/home/tinexus";
}

FileSidebarWidget::FileSidebarWidget() {
    refresh_locations();
}

void FileSidebarWidget::refresh_locations() {
    m_entries.clear();
    std::string home = get_home_dir();

    // Section 1: FAVORITES
    m_entries.push_back({"FAVORITES", "", txui::IconType::File, txui::Rect{}, false, true});
    m_entries.push_back({"Home",        home,                           txui::IconType::Home, txui::Rect{}, false, false});
    m_entries.push_back({"Desktop",     home + "/Desktop",              txui::IconType::Apps, txui::Rect{}, false, false});
    m_entries.push_back({"Documents",   home + "/Documents",            txui::IconType::File, txui::Rect{}, false, false});
    m_entries.push_back({"Downloads",   home + "/Downloads",            txui::IconType::Downloads, txui::Rect{}, false, false});
    m_entries.push_back({"Apps",        "/opt/tinexus-apps",            txui::IconType::Apps, txui::Rect{}, false, false});

    // Section 2: LOCATIONS
    m_entries.push_back({"LOCATIONS", "", txui::IconType::File, txui::Rect{}, false, true});
    m_entries.push_back({"File System", "/", txui::IconType::Folder, txui::Rect{}, false, false});

    // Check for mounted drives in /mnt or /media
    for (const auto& loc : {"/mnt", "/media"}) {
        std::error_code ec;
        if (std::filesystem::exists(loc, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(loc, ec)) {
                if (entry.is_directory(ec)) {
                    m_entries.push_back({entry.path().filename().string(), entry.path(), txui::IconType::Folder, txui::Rect{}, false, false});
                }
            }
        }
    }

    // Section 3: RECENTS
    m_entries.push_back({"RECENTS", "", txui::IconType::File, txui::Rect{}, false, true});
    m_entries.push_back({"Recent (Session)", home + "/.recents_view", txui::IconType::File, txui::Rect{}, false, false});

    // Section 4: TRASH
    m_entries.push_back({"Trash", home + "/.local/share/Trash/files", txui::IconType::Trash, txui::Rect{}, false, false});

    mark_needs_layout();
    mark_needs_paint();
}

void FileSidebarWidget::set_active_path(const std::filesystem::path& path) noexcept {
    if (m_active_path != path) {
        m_active_path = path;
        mark_needs_paint();
    }
}

txui::Size FileSidebarWidget::measure_override(const txui::Constraints& constraints) noexcept {
    return constraints.constrain(txui::Size(170.0, constraints.max_height));
}

void FileSidebarWidget::layout_override(const txui::Rect& frame) noexcept {
    double y = frame.top() + 10.0;
    const double w = frame.width();

    for (auto& item : m_entries) {
        if (item.is_section_header) {
            item.rect = txui::Rect(frame.left() + 16.0, y + 4.0, w - 32.0, 18.0);
            y += 24.0;
        } else {
            item.rect = txui::Rect(frame.left() + 8.0, y, w - 16.0, 28.0);
            y += 32.0;
        }
    }
}

void FileSidebarWidget::paint_override(txui::Painter& painter) const noexcept {
    const double gx = frame().left();
    const double gy = frame().top();
    const double W  = frame().width();
    const double H  = frame().height();

    // 1. Sidebar Background & Right Divider
    painter.fill_rect(frame(), txui::Color(22, 24, 34, 255));
    painter.fill_rect(txui::Rect(gx + W - 1.0, gy, 1.0, H), txui::Color(42, 45, 60, 255));

    // 2. Sidebar Items
    for (const auto& item : m_entries) {
        if (item.is_section_header) {
            painter.draw_text(txui::Point(item.rect.left(), item.rect.top() + 3.0),
                              item.name, txui::Color(105, 110, 130, 220), 10.0, true);
        } else {
            bool is_active = (!item.path.empty() && m_active_path == item.path);

            if (is_active) {
                painter.fill_rounded_rect(item.rect, 6.0, txui::Color(55, 120, 240, 220));
            } else if (item.hovered) {
                painter.fill_rounded_rect(item.rect, 6.0, txui::Color(255, 255, 255, 12));
            }

            // Draw miniature icon
            const double ic_x = item.rect.left() + 8.0;
            const double ic_y = item.rect.top() + 7.0;
            txui::Color ic_col = is_active ? txui::Color(255, 255, 255, 255) : txui::Color(120, 160, 245, 240);

            if (item.icon_type == txui::IconType::Home) {
                painter.fill_rounded_rect(txui::Rect(ic_x, ic_y + 4.0, 14.0, 10.0), 2.0, ic_col);
                // Roof
                painter.fill_rounded_rect(txui::Rect(ic_x + 2.0, ic_y, 10.0, 5.0), 1.0, ic_col);
            } else if (item.icon_type == txui::IconType::Trash) {
                painter.fill_rounded_rect(txui::Rect(ic_x + 1.0, ic_y + 3.0, 12.0, 11.0), 2.0,
                                          is_active ? txui::Color(255, 255, 255, 255) : txui::Color(240, 100, 100, 230));
            } else {
                painter.fill_rounded_rect(txui::Rect(ic_x, ic_y + 1.0, 14.0, 12.0), 2.0, ic_col);
            }

            txui::Color text_col = is_active ? txui::Color(255, 255, 255, 255) : txui::Color(210, 215, 230, 240);
            painter.draw_text(txui::Point(item.rect.left() + 28.0, item.rect.top() + 6.5),
                              item.name, text_col, 12.0, is_active);
        }
    }
}

bool FileSidebarWidget::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerMove) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;
        bool changed = false;

        for (auto& item : m_entries) {
            if (item.is_section_header) continue;
            bool h = item.rect.contains(px, py);
            if (h != item.hovered) {
                item.hovered = h;
                changed = true;
            }
        }
        if (changed) mark_needs_paint();
        return changed;
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        for (const auto& item : m_entries) {
            if (item.is_section_header) continue;
            if (item.rect.contains(px, py)) {
                set_active_path(item.path);
                if (on_location_selected) {
                    on_location_selected(item.path);
                }
                return true;
            }
        }
    }

    return false;
}

} // namespace tinexus::files::ui
