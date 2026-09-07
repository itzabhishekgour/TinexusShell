#include "files/ui/FileContextMenu.hpp"
#include "files/TagManager.hpp"
#include "files/FileClipboard.hpp"
#include <txui/render/FontMetrics.hpp>
#include <algorithm>

namespace tinexus::files::ui {

FileContextMenu::FileContextMenu() {
}

void FileContextMenu::show_at(double x, double y, const std::filesystem::path& path, bool is_background) {
    m_pos = txui::Point(x, y);
    m_target_path = path;
    m_is_background = is_background;
    m_is_open = true;

    m_options.clear();
    if (m_is_background) {
        m_options.push_back({FileContextAction::NewFolder, "New Folder", true, {}, false});
        m_options.push_back({FileContextAction::NewFile,   "New Document", true, {}, false});
        bool can_paste = FileClipboard::instance().has_items();
        m_options.push_back({FileContextAction::Paste,     "Paste", can_paste, {}, false});
        m_options.push_back({FileContextAction::Refresh,   "Refresh", true, {}, false});
    } else {
        m_options.push_back({FileContextAction::Open,      "Open", true, {}, false});
        m_options.push_back({FileContextAction::QuickLook, "Quick Look (Space)", true, {}, false});
        m_options.push_back({FileContextAction::Rename,    "Rename (F2)", true, {}, false});
        m_options.push_back({FileContextAction::Copy,      "Copy (Ctrl+C)", true, {}, false});
        m_options.push_back({FileContextAction::Cut,       "Cut (Ctrl+X)", true, {}, false});
        m_options.push_back({FileContextAction::Trash,     "Move to Trash", true, {}, false});
        m_options.push_back({FileContextAction::GetInfo,   "Get Info", true, {}, false});
    }

    mark_needs_layout();
    mark_needs_paint();

    if (frame().width() > 0.0 && frame().height() > 0.0) {
        layout_override(frame());
    } else {
        layout_override(txui::Rect(0.0, 0.0, 1920.0, 1080.0));
    }
}

void FileContextMenu::dismiss() noexcept {
    if (m_is_open) {
        m_is_open = false;
        mark_needs_paint();
        if (on_dismiss) on_dismiss();
    }
}

txui::Size FileContextMenu::measure_override(const txui::Constraints& constraints) noexcept {
    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void FileContextMenu::layout_override(const txui::Rect& f) noexcept {
    if (!m_is_open) return;

    const double menu_w = 195.0;
    const double menu_h = m_is_background ? 120.0 : 255.0;

    double mx = m_pos.x;
    double my = m_pos.y;

    if (mx + menu_w > f.right()) mx = f.right() - menu_w - 4.0;
    if (my + menu_h > f.bottom()) my = f.bottom() - menu_h - 4.0;
    if (mx < f.left()) mx = f.left() + 4.0;
    if (my < f.top()) my = f.top() + 4.0;

    m_menu_rect = txui::Rect(mx, my, menu_w, menu_h);

    double opt_y = my + 8.0;
    const double opt_h = 24.0;

    for (auto& opt : m_options) {
        opt.rect = txui::Rect(mx + 6.0, opt_y, menu_w - 12.0, opt_h);
        opt_y += opt_h;
    }

    if (!m_is_background) {
        opt_y += 8.0; // after divider
        double dot_x = mx + 16.0;
        double dot_y = opt_y + 16.0;

        m_tag_dots.clear();
        const auto& tags = TagManager::available_tags();
        for (const auto& tag : tags) {
            txui::Rect dot_r(dot_x - 3.0, dot_y - 3.0, 18.0, 18.0);
            m_tag_dots.push_back({tag.name, dot_r});
            dot_x += 20.0;
        }

        m_clear_tag_rect = txui::Rect(mx + 16.0, dot_y + 20.0, menu_w - 32.0, 20.0);
    }
}

void FileContextMenu::paint_override(txui::Painter& painter) const noexcept {
    if (!m_is_open) return;

    // 1. Popover Shadow & Card Background
    painter.fill_rounded_rect(txui::Rect(m_menu_rect.left() - 2.0, m_menu_rect.top() - 2.0,
                                         m_menu_rect.width() + 4.0, m_menu_rect.height() + 4.0),
                              8.0, txui::Color(55, 60, 80, 100));
    painter.fill_rounded_rect(m_menu_rect, 8.0, txui::Color(32, 34, 46, 255));

    // 2. Action Options
    for (const auto& opt : m_options) {
        if (opt.hovered && opt.enabled) {
            painter.fill_rounded_rect(opt.rect, 4.0, txui::Color(55, 120, 240, 200));
        }

        txui::Color fg = opt.enabled ? (opt.hovered ? txui::Color(255, 255, 255, 255)
                                                    : txui::Color(230, 235, 250, 255))
                                     : txui::Color(115, 120, 138, 180);
        painter.draw_text(txui::Point(opt.rect.left() + 10.0, opt.rect.top() + 5.0),
                          opt.label, fg, 11.5);
    }

    if (!m_is_background) {
        // Divider
        double div_y = m_menu_rect.top() + 8.0 + static_cast<double>(m_options.size()) * 24.0 + 2.0;
        painter.fill_rect(txui::Rect(m_menu_rect.left() + 8.0, div_y, m_menu_rect.width() - 16.0, 1.0),
                          txui::Color(55, 58, 75, 255));

        // Tags label & dots
        painter.draw_text(txui::Point(m_menu_rect.left() + 16.0, div_y + 6.0),
                          "Tags:", txui::Color(140, 145, 165, 220), 10.5, true);

        const auto& tags = TagManager::available_tags();
        for (size_t i = 0; i < m_tag_dots.size() && i < tags.size(); ++i) {
            const auto& dot = m_tag_dots[i];
            painter.fill_circle(txui::Point(dot.second.left() + 9.0, dot.second.top() + 9.0),
                                6.5, tags[i].color);
        }

        // Clear tag button
        txui::Color clr_col = m_clear_tag_hovered ? txui::Color(220, 225, 240, 255)
                                                  : txui::Color(160, 165, 185, 200);
        painter.draw_text(txui::Point(m_clear_tag_rect.left() + 4.0, m_clear_tag_rect.top() + 4.0),
                          "Remove Tag", clr_col, 10.5);
    }
}

bool FileContextMenu::handle_event(const txui::Event& event) noexcept {
    if (!m_is_open) return false;

    if (event.type == txui::EventType::KeyDown) {
        if (event.keyboard.key == txui::Key::Escape) {
            dismiss();
            return true;
        }
    }

    if (event.type == txui::EventType::PointerMove) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;
        bool changed = false;

        for (auto& opt : m_options) {
            bool h = opt.rect.contains(px, py);
            if (h != opt.hovered) {
                opt.hovered = h;
                changed = true;
            }
        }

        bool ch = m_clear_tag_rect.contains(px, py);
        if (ch != m_clear_tag_hovered) {
            m_clear_tag_hovered = ch;
            changed = true;
        }

        if (changed) mark_needs_paint();
        return m_menu_rect.contains(px, py);
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        const double px = event.pointer.x;
        const double py = event.pointer.y;

        if (!m_menu_rect.contains(px, py)) {
            dismiss();
            return true;
        }

        for (const auto& opt : m_options) {
            if (opt.rect.contains(px, py) && opt.enabled) {
                auto act = opt.action;
                auto target = m_target_path;
                dismiss();
                if (on_action_selected) {
                    on_action_selected(act, target);
                }
                return true;
            }
        }

        for (const auto& dot : m_tag_dots) {
            if (dot.second.contains(px, py)) {
                auto tag_name = dot.first;
                auto target = m_target_path;
                dismiss();
                if (on_tag_selected) {
                    on_tag_selected(tag_name, target);
                }
                return true;
            }
        }

        if (m_clear_tag_rect.contains(px, py)) {
            auto target = m_target_path;
            dismiss();
            if (on_action_selected) {
                on_action_selected(FileContextAction::ClearTag, target);
            }
            return true;
        }

        return true;
    }

    return false;
}

} // namespace tinexus::files::ui
