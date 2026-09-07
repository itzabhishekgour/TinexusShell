#include <launcher/LauncherWidget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/FontMetrics.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace tinexus::launcher {

namespace tokens {
    constexpr txui::Color BG_BACKDROP_T {  2,   2,   8, 240};
    constexpr txui::Color BG_BACKDROP_B {  5,   3,  15, 240};
    constexpr txui::Color BG_PANEL_T    { 18,  18,  30, 252};
    constexpr txui::Color BG_PANEL_B    { 12,  12,  22, 252};
    constexpr txui::Color PANEL_BORDER  {130, 130, 220,  35};
    constexpr txui::Color ACCENT        {107, 140, 239, 255};
    constexpr txui::Color SEL_APP       { 59, 130, 246, 200};
    constexpr txui::Color SEL_SYS       {239,  68,  68, 190};
    constexpr txui::Color SEL_CALC      { 16, 185, 129, 200};
    constexpr txui::Color TXT_PRI       {240, 240, 248, 255};
    constexpr txui::Color TXT_SEC       {180, 180, 210, 200};
    constexpr txui::Color TXT_DIM       {120, 120, 150, 160};

    constexpr double PANEL_W      = 680.0;
    constexpr double PANEL_RAD    = 18.0;
    constexpr double HEADER_H     = 34.0;
    constexpr double SEARCH_H     = 56.0;
    constexpr double ROW_H        = 48.0;
    constexpr double ROW_GAP      =  3.0;
    constexpr size_t MAX_ROWS     = 7;
}

static std::string to_lower_str(std::string_view sv) {
    std::string res;
    res.reserve(sv.size());
    for (char c : sv) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

LauncherWidget::LauncherWidget() noexcept {
    m_search_input = txui::make_ref<txui::TextInput>("Search apps, commands, or calculate (e.g. 42 * 8)...");
    m_search_input->set_font_size(15.0);
    m_search_input->set_corner_radius(9.0);
    m_search_input->set_focused(true);

    m_search_input->set_on_text_changed([this](std::string_view text) {
        set_query(text);
    });

    add_child(m_search_input);
}

void LauncherWidget::set_all_apps(std::vector<AppItem> apps) noexcept {
    m_all_apps = std::move(apps);
    refresh_results();
}

void LauncherWidget::set_recent_launches(std::vector<AppItem> recents) noexcept {
    m_recent_launches = std::move(recents);
    refresh_results();
}

void LauncherWidget::set_query(std::string_view query) noexcept {
    if (m_query != query) {
        m_query = std::string(query);
        if (m_search_input->text() != m_query) {
            m_search_input->set_text(m_query);
        }
        m_selected_index = 0;
        refresh_results();
        mark_needs_paint();
    }
}

void LauncherWidget::set_selected_index(size_t index) noexcept {
    if (m_results.empty()) {
        m_selected_index = 0;
    } else {
        m_selected_index = std::min(index, m_results.size() - 1);
    }
    mark_needs_paint();
}

bool LauncherWidget::try_eval_calc(std::string_view query, double& result) noexcept {
    bool has_op = false;
    bool has_dig = false;
    for (char c : query) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') has_dig = true;
        if (c == '+' || c == '-' || c == '*' || c == '/') has_op = true;
        if (!std::isdigit(static_cast<unsigned char>(c)) &&
            c != '+' && c != '-' && c != '*' && c != '/' &&
            c != '.' && c != ' ' && c != '(' && c != ')') {
            return false;
        }
    }
    if (!has_dig || !has_op) return false;

    double a{0.0}, b{0.0};
    char op{'?'};
    std::istringstream ss{std::string(query)};
    if (!(ss >> a)) return false;
    if (!(ss >> op)) return false;
    if (!(ss >> b)) return false;
    if (std::abs(b) < 1e-9 && op == '/') return false;

    switch (op) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '*': result = a * b; break;
        case '/': result = a / b; break;
        default:  return false;
    }
    return true;
}

std::vector<AppItem> LauncherWidget::get_system_actions(std::string_view lq) noexcept {
    struct SA { const char* cmd; const char* label; const char* desc; const char* icon; };
    constexpr SA kActions[] = {
        {"lock",     "Lock Screen",  "Lock the current desktop session", "system-lock-screen"},
        {"shutdown", "Shut Down",    "Safely power off the computer",    "system-shutdown"},
        {"reboot",   "Restart",      "Reboot the platform",              "system-reboot"},
        {"sleep",    "Sleep",        "Suspend system to RAM",            "system-suspend"},
        {"logout",   "Log Out",      "End current user session",         "system-log-out"},
    };
    std::vector<AppItem> out;
    for (const auto& a : kActions) {
        std::string cmd = to_lower_str(a.cmd);
        std::string label = to_lower_str(a.label);
        if (lq.empty() || cmd.find(lq) != std::string::npos || label.find(lq) != std::string::npos) {
            out.push_back({a.label, a.cmd, a.desc, false, a.icon, ResultKind::System});
        }
    }
    return out;
}

txui::IconType LauncherWidget::resolve_icon_type(const AppItem& item) noexcept {
    if (item.kind == ResultKind::System) {
        return txui::IconType::Settings;
    }

    std::string name = to_lower_str(item.name);
    std::string exec = to_lower_str(item.exec);
    std::string icon = to_lower_str(item.icon);

    if (item.is_terminal || name.find("terminal") != std::string::npos ||
        exec.find("terminal") != std::string::npos || exec.find("foot") != std::string::npos ||
        icon.find("terminal") != std::string::npos) {
        return txui::IconType::Terminal;
    }
    if (name.find("file") != std::string::npos || exec.find("files") != std::string::npos ||
        icon.find("file-manager") != std::string::npos || icon.find("folder") != std::string::npos) {
        return txui::IconType::Folder;
    }
    if (name.find("setting") != std::string::npos || exec.find("setting") != std::string::npos ||
        icon.find("preferences") != std::string::npos || icon.find("gear") != std::string::npos) {
        return txui::IconType::Gear;
    }
    if (name.find("monitor") != std::string::npos || name.find("htop") != std::string::npos ||
        exec.find("monitor") != std::string::npos || icon.find("system-monitor") != std::string::npos) {
        return txui::IconType::BarChart;
    }
    if (name.find("package") != std::string::npos || name.find("software") != std::string::npos ||
        exec.find("pkg") != std::string::npos || icon.find("software-install") != std::string::npos) {
        return txui::IconType::Package;
    }

    return txui::IconType::Executable;
}

void LauncherWidget::refresh_results() noexcept {
    m_results.clear();
    std::string lq = to_lower_str(m_query);

    // 1. Inline calculator check
    double calc_result{0.0};
    if (!lq.empty() && try_eval_calc(lq, calc_result)) {
        std::ostringstream oss;
        if (std::abs(calc_result - std::round(calc_result)) < 1e-9) {
            oss << static_cast<long long>(std::round(calc_result));
        } else {
            oss << calc_result;
        }
        std::string ans = oss.str();
        m_results.push_back({"= " + ans, ans, m_query + " = " + ans, false, "accessories-calculator", ResultKind::Calculator});
    }

    // 2. System actions
    if (!lq.empty()) {
        auto sys = get_system_actions(lq);
        m_results.insert(m_results.end(), sys.begin(), sys.end());
    }

    // 3. Applications
    if (lq.empty()) {
        // Empty query: Show recent launches if available; otherwise show default suggested system apps
        if (!m_recent_launches.empty()) {
            for (const auto& r : m_recent_launches) {
                m_results.push_back(r);
            }
        } else {
            for (size_t i = 0; i < std::min(m_all_apps.size(), size_t{5}); ++i) {
                m_results.push_back(m_all_apps[i]);
            }
        }
    } else {
        for (const auto& app : m_all_apps) {
            if (to_lower_str(app.name).find(lq) != std::string::npos ||
                to_lower_str(app.exec).find(lq) != std::string::npos ||
                to_lower_str(app.description).find(lq) != std::string::npos) {
                m_results.push_back(app);
            }
        }
    }

    if (m_selected_index >= m_results.size()) {
        m_selected_index = m_results.empty() ? 0 : m_results.size() - 1;
    }
}

void LauncherWidget::launch_selected() noexcept {
    if (m_results.empty()) {
        if (!m_query.empty() && m_on_launch) {
            AppItem custom_exec{m_query, m_query, "Custom Command", false, "", ResultKind::App};
            m_on_launch(custom_exec);
        }
        return;
    }

    if (m_selected_index < m_results.size() && m_on_launch) {
        m_on_launch(m_results[m_selected_index]);
    }
}

txui::Size LauncherWidget::measure_override(const txui::Constraints& constraints) noexcept {
    return txui::Size(constraints.max_width, constraints.max_height);
}

void LauncherWidget::layout_override(const txui::Rect& frame) noexcept {
    using namespace tokens;
    const double W  = frame.width();
    const double H  = frame.height();
    const double cx = W * 0.5;
    const double cy = H * 0.5;

    const size_t n_rows = std::min(m_results.size(), MAX_ROWS);
    const double header_extra = (m_query.empty() && !m_results.empty()) ? 24.0 : 0.0;
    const double rows_h = n_rows > 0 ? (header_extra + n_rows * ROW_H + (n_rows - 1) * ROW_GAP + 14.0) : (m_query.empty() ? 44.0 : 0.0);
    const double panel_h = HEADER_H + SEARCH_H + rows_h;
    const double panel_x = cx - PANEL_W * 0.5;
    const double panel_y = cy - panel_h * 0.5 - 20.0;

    const double srch_y = panel_y + HEADER_H;
    const double input_x = panel_x + 52.0;
    const double input_y = srch_y + (SEARCH_H - 38.0) * 0.5;
    const double input_w = PANEL_W - 66.0;
    const double input_h = 38.0;

    m_search_input->measure(txui::Constraints(input_w, input_w, input_h, input_h));
    m_search_input->layout(txui::Rect(input_x, input_y, input_w, input_h));
}

void LauncherWidget::paint_override(txui::Painter& painter) const noexcept {
    using namespace tokens;
    const double W  = frame().width();
    const double H  = frame().height();
    const double cx = W * 0.5;
    const double cy = H * 0.5;

    // 1. Full-screen dimmed glass backdrop
    painter.fill_gradient_rect(frame(), BG_BACKDROP_T, BG_BACKDROP_B);

    // 2. Geometry calculations
    const size_t n_rows = std::min(m_results.size(), MAX_ROWS);
    const bool show_recent_header = m_query.empty() && !m_results.empty();
    const double header_extra = show_recent_header ? 24.0 : 0.0;
    const double rows_h = n_rows > 0 ? (header_extra + n_rows * ROW_H + (n_rows - 1) * ROW_GAP + 14.0) : (m_query.empty() ? 44.0 : 0.0);
    const double panel_h = HEADER_H + SEARCH_H + rows_h;
    const double panel_x = cx - PANEL_W * 0.5;
    const double panel_y = cy - panel_h * 0.5 - 20.0;

    // 3. Panel card glow aura + glass container
    painter.fill_circle(txui::Point(cx, panel_y + panel_h * 0.5), PANEL_W * 0.55, txui::Color(80, 60, 180, 10));
    painter.fill_rounded_rect(txui::Rect(panel_x - 1.0, panel_y - 1.0, PANEL_W + 2.0, panel_h + 2.0),
                              PANEL_RAD + 1.0, PANEL_BORDER);
    painter.fill_gradient_rounded_rect(txui::Rect(panel_x, panel_y, PANEL_W, panel_h),
                                       PANEL_RAD, BG_PANEL_T, BG_PANEL_B);
    // Subtle top specular edge
    painter.fill_rounded_rect(txui::Rect(panel_x + 4.0, panel_y + 1.0, PANEL_W - 8.0, 1.0),
                              0.5, txui::Color(255, 255, 255, 22));

    // 4. Header bar
    painter.fill_gradient_rounded_rect(
        txui::Rect(panel_x, panel_y, PANEL_W, HEADER_H),
        PANEL_RAD,
        txui::Color(24, 24, 42, 255),
        txui::Color(16, 16, 30, 255));
    // Glowing accent status dot
    painter.fill_circle(txui::Point(panel_x + 22.0, panel_y + 17.0), 4.5, ACCENT);
    painter.draw_glow(txui::Point(panel_x + 22.0, panel_y + 17.0), 4.5, 14.0, ACCENT);
    painter.draw_text(txui::Point(panel_x + 36.0, panel_y + 10.0),
                      "TINEXUS  Command Palette", TXT_DIM, 12.0);

    // Escape badge button
    painter.fill_rounded_rect(txui::Rect(panel_x + PANEL_W - 52.0, panel_y + 8.0, 40.0, 18.0),
                              4.0, txui::Color(45, 45, 65, 180));
    painter.draw_text(txui::Point(panel_x + PANEL_W - 44.0, panel_y + 11.0), "Esc", TXT_DIM, 11.0);

    // 5. Search row
    const double srch_y = panel_y + HEADER_H;
    painter.fill_gradient_rect(
        txui::Rect(panel_x, srch_y, PANEL_W, SEARCH_H),
        txui::Color(26, 26, 44, 255),
        txui::Color(20, 20, 36, 255));

    // Search magnifying glass vector glyph
    const double mg_cx = panel_x + 30.0;
    const double mg_cy = srch_y + SEARCH_H * 0.5 - 2.0;
    painter.draw_circle(txui::Point(mg_cx, mg_cy), 7.5, 1.6, ACCENT);
    painter.draw_line(txui::Point(mg_cx + 5.0, mg_cy + 5.0),
                      txui::Point(mg_cx + 10.5, mg_cy + 10.5), 2.0, ACCENT);

    // Embedded TextInput widget
    m_search_input->paint(painter);

    // Bottom subtle divider under search input
    painter.fill_gradient_rect(
        txui::Rect(panel_x + 14.0, srch_y + SEARCH_H - 1.0, PANEL_W - 28.0, 1.0),
        txui::Color(107, 140, 239, 45),
        txui::Color(107, 140, 239, 0), true);

    // 6. Result rows
    double cur_y = srch_y + SEARCH_H + 6.0;

    if (show_recent_header) {
        painter.draw_text(txui::Point(panel_x + 16.0, cur_y + 2.0),
                          "RECENT APPLICATIONS", TXT_DIM, 10.5);
        cur_y += 20.0;
    }

    if (!m_results.empty()) {
        for (size_t i = 0; i < n_rows; ++i) {
            const auto& item = m_results[i];
            const bool sel   = (i == m_selected_index);
            const double ry  = cur_y + static_cast<double>(i) * (ROW_H + ROW_GAP);

            txui::Color cat_col = (item.kind == ResultKind::Calculator) ? SEL_CALC :
                                  (item.kind == ResultKind::System)     ? SEL_SYS  : SEL_APP;

            // Highlight background
            if (sel) {
                painter.fill_gradient_rounded_rect(
                    txui::Rect(panel_x + 8.0, ry, PANEL_W - 16.0, ROW_H), 8.0,
                    txui::Color(cat_col.r(), cat_col.g(), cat_col.b(), 55),
                    txui::Color(cat_col.r(), cat_col.g(), cat_col.b(), 20));
                // Active left accent indicator pill
                painter.fill_rounded_rect(
                    txui::Rect(panel_x + 10.0, ry + 8.0, 3.5, ROW_H - 16.0), 1.75,
                    cat_col);
            }

            // Icon representation
            const double icon_x = panel_x + 22.0;
            const double icon_y = ry + (ROW_H - 24.0) * 0.5;

            if (item.kind == ResultKind::Calculator) {
                // Circular emerald badge with math symbol
                const double calc_cx = icon_x + 12.0;
                const double calc_cy = icon_y + 12.0;
                painter.fill_circle(txui::Point(calc_cx, calc_cy), 12.0, txui::Color(16, 185, 129, 60));
                painter.draw_circle(txui::Point(calc_cx, calc_cy), 12.0, 1.2, txui::Color(16, 185, 129, 210));
                painter.draw_text(txui::Point(calc_cx - 5.0, calc_cy - 7.0), "=", txui::Color(255, 255, 255, 255), 14.0);
            } else {
                // Canonical TxUI Icon rendering
                txui::IconType icon_type = resolve_icon_type(item);
                txui::Icon::render(painter, icon_type, txui::Rect(icon_x, icon_y, 24.0, 24.0));
            }

            // Item Name
            const double text_y = ry + (ROW_H - 14.0) * 0.5;
            painter.draw_text(txui::Point(panel_x + 56.0, text_y),
                              item.name, sel ? TXT_PRI : TXT_SEC, 14.0);

            // Item Description or Category Pill
            if (item.kind == ResultKind::Calculator) {
                painter.fill_rounded_rect(txui::Rect(panel_x + PANEL_W - 190.0, ry + 13.0, 95.0, 20.0),
                                          5.0, txui::Color(16, 185, 129, 40));
                painter.draw_text(txui::Point(panel_x + PANEL_W - 180.0, ry + 17.0),
                                  "Calculator", txui::Color(52, 211, 153, 240), 11.0);
            } else if (item.kind == ResultKind::System) {
                painter.fill_rounded_rect(txui::Rect(panel_x + PANEL_W - 180.0, ry + 13.0, 85.0, 20.0),
                                          5.0, txui::Color(239, 68, 68, 40));
                painter.draw_text(txui::Point(panel_x + PANEL_W - 170.0, ry + 17.0),
                                  "System Action", txui::Color(248, 113, 113, 240), 11.0);
            } else if (!item.description.empty()) {
                double max_desc_w = 260.0;
                std::string desc = item.description;
                double text_w = txui::FontMetrics::measure(desc, 11.0).width;
                if (text_w > max_desc_w) {
                    while (!desc.empty() && txui::FontMetrics::measure(desc + "...", 11.0).width > max_desc_w) {
                        desc.pop_back();
                    }
                    desc += "...";
                }
                const double desc_x = panel_x + PANEL_W - txui::FontMetrics::measure(desc, 11.0).width - 65.0;
                if (desc_x > panel_x + 220.0) {
                    painter.draw_text(txui::Point(desc_x, text_y + 1.0), desc, TXT_DIM, 11.0);
                }
            }

            // Right selection badge / shortcut index
            if (sel) {
                painter.fill_rounded_rect(txui::Rect(panel_x + PANEL_W - 55.0, ry + 13.0, 42.0, 20.0),
                                          5.0, txui::Color(cat_col.r(), cat_col.g(), cat_col.b(), 70));
                painter.draw_text(txui::Point(panel_x + PANEL_W - 47.0, ry + 17.0),
                                  "↵ Go", TXT_PRI, 11.0);
            } else if (i < 9) {
                painter.draw_text(txui::Point(panel_x + PANEL_W - 30.0, text_y + 1.0),
                                  std::to_string(i + 1), TXT_DIM, 11.0);
            }
        }
    } else if (!m_query.empty()) {
        // No results message
        painter.draw_text(txui::Point(panel_x + 30.0, cur_y + 14.0),
                          "No matching applications or commands found", TXT_DIM, 13.0);
    }

    // 7. Bottom navigation hint footer
    const double tip_y = panel_y + panel_h + 12.0;
    const std::string tips = "↑↓ Navigate    ↵ Launch    Tab Cycle    Esc Dismiss";
    const double tips_w = txui::FontMetrics::measure(tips, 11.0).width;
    painter.draw_text(txui::Point(cx - tips_w * 0.5, tip_y),
                      tips, txui::Color(130, 130, 165, 140), 11.0);
}

bool LauncherWidget::handle_event(const txui::Event& event) noexcept {
    // 1. Intercept navigation keys BEFORE TextInput
    if (event.type == txui::EventType::KeyDown) {
        const bool ctrl = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Ctrl);

        if (event.keyboard.key == txui::Key::Escape) {
            if (m_on_close) {
                m_on_close();
            }
            return true;
        }

        if (event.keyboard.key == txui::Key::Up) {
            if (m_selected_index > 0) {
                m_selected_index--;
                mark_needs_paint();
            }
            return true;
        }

        if (event.keyboard.key == txui::Key::Down) {
            if (!m_results.empty() && m_selected_index + 1 < m_results.size()) {
                m_selected_index++;
                mark_needs_paint();
            }
            return true;
        }

        if (event.keyboard.key == txui::Key::Tab) {
            if (!m_results.empty()) {
                m_selected_index = (m_selected_index + 1) % m_results.size();
                mark_needs_paint();
            }
            return true;
        }

        if (event.keyboard.key == txui::Key::Enter) {
            launch_selected();
            return true;
        }

        // Ctrl + 1..9 fast shortcut
        if (ctrl && event.keyboard.key >= txui::Key::N1 && event.keyboard.key <= txui::Key::N9) {
            size_t idx = static_cast<size_t>(
                static_cast<int>(event.keyboard.key) - static_cast<int>(txui::Key::N1));
            if (idx < m_results.size()) {
                m_selected_index = idx;
                launch_selected();
                return true;
            }
        }

        // 2. Forward all text editing, typing, backspace, cursor movement to TextInput
        if (m_search_input && m_search_input->handle_event(event)) {
            return true;
        }
    }

    // 3. Pointer event handling for row clicks / hover
    if (event.type == txui::EventType::PointerButtonPress) {
        if (m_search_input && m_search_input->frame().contains(
            txui::Point(event.pointer.x, event.pointer.y))) {
            return m_search_input->handle_event(event);
        }

        // Hit-test rows
        using namespace tokens;
        const double W  = frame().width();
        const double H  = frame().height();
        const double cx = W * 0.5;
        const double cy = H * 0.5;
        const size_t n_rows = std::min(m_results.size(), MAX_ROWS);
        const bool show_recent_header = m_query.empty() && !m_results.empty();
        const double header_extra = show_recent_header ? 24.0 : 0.0;
        const double rows_h = n_rows > 0 ? (header_extra + n_rows * ROW_H + (n_rows - 1) * ROW_GAP + 14.0) : 0.0;
        const double panel_h = HEADER_H + SEARCH_H + rows_h;
        const double panel_x = cx - PANEL_W * 0.5;
        const double panel_y = cy - panel_h * 0.5 - 20.0;
        const double cur_y   = panel_y + HEADER_H + SEARCH_H + 6.0 + (show_recent_header ? 20.0 : 0.0);

        for (size_t i = 0; i < n_rows; ++i) {
            const double ry = cur_y + static_cast<double>(i) * (ROW_H + ROW_GAP);
            txui::Rect row_rect(panel_x + 8.0, ry, PANEL_W - 16.0, ROW_H);
            if (row_rect.contains(txui::Point(event.pointer.x, event.pointer.y))) {
                m_selected_index = i;
                launch_selected();
                return true;
            }
        }
    }

    if (event.type == txui::EventType::PointerMove) {
        using namespace tokens;
        const double W  = frame().width();
        const double H  = frame().height();
        const double cx = W * 0.5;
        const double cy = H * 0.5;
        const size_t n_rows = std::min(m_results.size(), MAX_ROWS);
        const bool show_recent_header = m_query.empty() && !m_results.empty();
        const double header_extra = show_recent_header ? 24.0 : 0.0;
        const double rows_h = n_rows > 0 ? (header_extra + n_rows * ROW_H + (n_rows - 1) * ROW_GAP + 14.0) : 0.0;
        const double panel_h = HEADER_H + SEARCH_H + rows_h;
        const double panel_x = cx - PANEL_W * 0.5;
        const double panel_y = cy - panel_h * 0.5 - 20.0;
        const double cur_y   = panel_y + HEADER_H + SEARCH_H + 6.0 + (show_recent_header ? 20.0 : 0.0);

        for (size_t i = 0; i < n_rows; ++i) {
            const double ry = cur_y + static_cast<double>(i) * (ROW_H + ROW_GAP);
            txui::Rect row_rect(panel_x + 8.0, ry, PANEL_W - 16.0, ROW_H);
            if (row_rect.contains(txui::Point(event.pointer.x, event.pointer.y))) {
                if (m_selected_index != i) {
                    m_selected_index = i;
                    mark_needs_paint();
                }
                return true;
            }
        }
    }

    return false;
}

} // namespace tinexus::launcher
