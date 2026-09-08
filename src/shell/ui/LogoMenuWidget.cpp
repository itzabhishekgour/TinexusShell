#include "shell/ui/LogoMenuWidget.hpp"
#include <txui/render/FontMetrics.hpp>

namespace tinexus::shell {

namespace {
    constexpr txui::Color CARD_BG       { 22,  24,  32, 248};
    constexpr txui::Color BORDER_LINE   {255, 255, 255,  15};
    constexpr txui::Color ROW_SELECT    { 59, 130, 246, 220};
    constexpr txui::Color TXT_PRI       {245, 245, 250, 255};
    constexpr txui::Color TXT_DIM       {120, 125, 140, 200};
    constexpr txui::Color ACCENT_BLUE   { 59, 130, 246, 255};
    constexpr txui::Color ACCENT_CYAN   { 56, 189, 248, 255};

    struct MenuItemDef {
        const char* title;
        bool separator;
        bool bold;
        txui::Color dot_col;
        const char* shortcut;
        LogoMenuAction action;
    };

    const MenuItemDef MENU_ITEMS[] = {
        {"About Tinexus",       false, true,  ACCENT_CYAN,                  "",       LogoMenuAction::AboutTinexus},
        {"---",                 true,  false, txui::Color(),                "",       LogoMenuAction::AboutTinexus},
        {"System Settings...",  false, false, ACCENT_BLUE,                  "",       LogoMenuAction::SystemSettings},
        {"App Store...",        false, false, ACCENT_CYAN,                  "",       LogoMenuAction::AppInstaller},
        {"Activity Monitor...",  false, false, txui::Color(168, 85, 247, 255),"",       LogoMenuAction::SystemMonitor},
        {"---",                 true,  false, txui::Color(),                "",       LogoMenuAction::AboutTinexus},
        {"Sleep",               false, false, txui::Color(245, 158, 11, 255), "",      LogoMenuAction::Sleep},
        {"Restart...",          false, false, ACCENT_CYAN,                  "",       LogoMenuAction::Restart},
        {"Shut Down...",        false, false, txui::Color(239, 68, 68, 255),"",       LogoMenuAction::ShutDown},
        {"---",                 true,  false, txui::Color(),                "",       LogoMenuAction::AboutTinexus},
        {"Lock Screen",         false, false, ACCENT_BLUE,                  "Ctrl+L", LogoMenuAction::LockScreen}
    };
    constexpr size_t MENU_ITEM_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
}

LogoMenuWidget::LogoMenuWidget() = default;

txui::Size LogoMenuWidget::measure_override(const txui::Constraints&) noexcept {
    return txui::Size(236.0, 260.0);
}

void LogoMenuWidget::layout_override(const txui::Rect&) noexcept {
}

void LogoMenuWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto& f = frame();
    const double mx = f.x(), my = f.y();
    const double mw = f.width(), mh = f.height();

    // 1. Drop shadow
    painter.fill_rounded_rect(txui::Rect(mx - 4.0, my + 6.0, mw + 8.0, mh + 4.0), 14.0, txui::Color(0, 0, 0, 95));

    // 2. Glassmorphic card body
    painter.fill_rounded_rect(txui::Rect(mx, my, mw, mh), 14.0, CARD_BG);
    painter.fill_rounded_rect(txui::Rect(mx - 1.0, my - 1.0, mw + 2.0, mh + 2.0), 15.0, BORDER_LINE);

    double cur_y = my + 8.0;
    int act_idx = 0;

    for (size_t i = 0; i < MENU_ITEM_COUNT; ++i) {
        if (MENU_ITEMS[i].separator) {
            painter.draw_line(txui::Point(mx + 12.0, cur_y + 4.0), txui::Point(mx + mw - 12.0, cur_y + 4.0), 1.0, BORDER_LINE);
            cur_y += 9.0;
        } else {
            bool sel = (m_hovered_index == act_idx);
            if (sel) {
                painter.fill_rounded_rect(txui::Rect(mx + 6.0, cur_y, mw - 12.0, 24.0), 6.0, ROW_SELECT);
            }

            // Leading accent dot
            painter.fill_circle(txui::Point(mx + 16.0, cur_y + 12.0), 3.5,
                                sel ? txui::Color(255, 255, 255, 255) : MENU_ITEMS[i].dot_col);

            painter.draw_text(txui::Point(mx + 28.0, cur_y + 5.0), MENU_ITEMS[i].title,
                              sel ? txui::Color(255, 255, 255, 255) : TXT_PRI, 12.5, MENU_ITEMS[i].bold);

            if (MENU_ITEMS[i].shortcut[0] != '\0') {
                double sc_w = txui::FontMetrics::measure(MENU_ITEMS[i].shortcut, 11.0).width;
                painter.draw_text(txui::Point(mx + mw - sc_w - 14.0, cur_y + 6.0),
                                  MENU_ITEMS[i].shortcut,
                                  sel ? txui::Color(220, 230, 255, 220) : TXT_DIM, 11.0);
            }

            cur_y += 25.0;
            act_idx++;
        }
    }
}

bool LogoMenuWidget::handle_event(const txui::Event& event) noexcept {
    const auto& f = frame();

    if (event.type == txui::EventType::PointerMove) {
        double mx = event.pointer.x, my = event.pointer.y;
        int new_hov = -1;

        if (f.contains(txui::Point(mx, my))) {
            double cur_y = f.y() + 8.0;
            int act_idx = 0;
            for (size_t i = 0; i < MENU_ITEM_COUNT; ++i) {
                if (MENU_ITEMS[i].separator) {
                    cur_y += 9.0;
                } else {
                    if (my >= cur_y && my <= cur_y + 24.0 && mx >= f.x() + 6.0 && mx <= f.x() + f.width() - 6.0) {
                        new_hov = act_idx;
                        break;
                    }
                    cur_y += 25.0;
                    act_idx++;
                }
            }
        }

        if (m_hovered_index != new_hov) {
            m_hovered_index = new_hov;
            mark_needs_paint();
        }
        return f.contains(txui::Point(mx, my));
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            if (m_hovered_index >= 0) {
                int act_idx = 0;
                for (size_t i = 0; i < MENU_ITEM_COUNT; ++i) {
                    if (!MENU_ITEMS[i].separator) {
                        if (act_idx == m_hovered_index) {
                            if (on_action_selected) {
                                on_action_selected(MENU_ITEMS[i].action);
                            }
                            return true;
                        }
                        act_idx++;
                    }
                }
            }
        }
    }

    return false;
}

} // namespace tinexus::shell
