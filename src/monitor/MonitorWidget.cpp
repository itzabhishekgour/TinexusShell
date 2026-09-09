#include "monitor/MonitorWidget.hpp"
#include "monitor/resource_monitor.hpp"
#include "monitor/process_controller.hpp"
#include <txui/input/Event.hpp>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

using namespace txui;

namespace tinexus::monitor {

namespace colors {
    constexpr Color BG_DARK         { 14,  14,  20, 255};
    constexpr Color CARD_BG         { 22,  22,  30, 240};
    constexpr Color CARD_BORDER     {255, 255, 255,  18};
    constexpr Color TXT_PRI         {245, 245, 250, 255};
    constexpr Color TXT_SEC         {160, 165, 185, 255};
    constexpr Color TXT_DIM         {110, 115, 135, 255};
    constexpr Color ACCENT_BLUE     { 59, 130, 246, 255}; // CPU
    constexpr Color ACCENT_PURPLE   {168,  85, 247, 255}; // RAM
    constexpr Color ACCENT_AMBER    {245, 158,  11, 255}; // Disk
    constexpr Color ACCENT_GREEN    { 34, 197,  94, 255}; // Network
    constexpr Color STATUS_ACTIVE   { 34, 197,  94, 255}; // Green
    constexpr Color STATUS_STANDBY  {245, 158,  11, 255}; // Amber
    constexpr Color STATUS_STOPPED  {239,  68,  68, 255}; // Red
    constexpr Color BADGE_ACTIVE_BG { 34, 197,  94,  30};
    constexpr Color BADGE_STANDBY_BG{245, 158,  11,  30};
    constexpr Color PROTECTED_BG    {234, 179,   8,  25};
    constexpr Color PROTECTED_BORDER{234, 179,   8,  60};
    constexpr Color PROTECTED_TXT   {253, 224,  71, 255};
    constexpr Color GHOST_BTN_BG    {255, 255, 255,  10};
    constexpr Color GHOST_BTN_BORDER{255, 255, 255,  28};
    constexpr Color ROW_SELECTED_BG { 59, 130, 246,  50};
    constexpr Color ROW_SELECTED_BDR{ 59, 130, 246, 120};
    constexpr Color DANGER_BG       {239,  68,  68, 220};
    constexpr Color WARNING_BANNER  {239,  68,  68,  40};
}

MonitorWidget::MonitorWidget() noexcept {
    m_current_uid = getuid();

    // 1. Top Segmented Control (Processes | Performance | Services)
    m_tabbar = make_ref<SegmentedControl>();
    m_tabbar->set_segments({"Processes", "Performance", "Services"});
    m_tabbar->set_selected_index(static_cast<size_t>(MonitorTab::Processes));
    m_tabbar->set_on_segment_selected([this](size_t idx) {
        switch_tab(static_cast<MonitorTab>(idx));
    });
    add_child(m_tabbar);

    // 2. Performance Tab Charts
    m_cpu_graph = make_ref<LineGraphWidget>("CPU Utilization");
    m_cpu_graph->add_series("CPU", colors::ACCENT_BLUE);
    m_cpu_graph->set_series_range(0, 0.0f, 100.0f);
    add_child(m_cpu_graph);

    m_memory_graph = make_ref<LineGraphWidget>("Memory & Swap");
    m_memory_graph->add_series("RAM", colors::ACCENT_PURPLE);
    m_memory_graph->set_series_range(0, 0.0f, 100.0f);
    add_child(m_memory_graph);

    m_disk_graph = make_ref<LineGraphWidget>("Disk Throughput (I/O)");
    m_disk_graph->add_series("Disk", colors::ACCENT_AMBER);
    m_disk_graph->set_series_range(0, 0.0f, 1000.0f);
    add_child(m_disk_graph);

    m_network_graph = make_ref<LineGraphWidget>("Network Activity");
    m_network_graph->add_series("Network", colors::ACCENT_GREEN);
    m_network_graph->set_series_range(0, 0.0f, 1000.0f);
    add_child(m_network_graph);

    for (size_t i = 0; i < 60; ++i) {
        m_cpu_graph->push_value(0, 0.0f);
        m_memory_graph->push_value(0, 0.0f);
        m_disk_graph->push_value(0, 0.0f);
        m_network_graph->push_value(0, 0.0f);
    }

    // 3. Processes Tab Controls: Live Search + Category Filter
    m_search_input = make_ref<TextInput>("Search processes...");
    m_search_input->set_on_text_changed([this](std::string_view text) {
        m_search_query = std::string(text);
        m_scroll_offset = 0;
        mark_needs_paint();
    });
    add_child(m_search_input);

    m_category_control = make_ref<SegmentedControl>();
    m_category_control->set_segments({"All", "My Processes", "System"});
    m_category_control->set_selected_index(static_cast<size_t>(ProcessCategory::All));
    m_category_control->set_on_segment_selected([this](size_t idx) {
        m_active_category = static_cast<ProcessCategory>(idx);
        m_scroll_offset = 0;
        mark_needs_paint();
    });
    add_child(m_category_control);

    // Initial telemetry collection
    refresh_telemetry();
}

void MonitorWidget::set_search_query(std::string query) noexcept {
    m_search_query = std::move(query);
    if (m_search_input) {
        m_search_input->set_text(m_search_query);
    }
    m_scroll_offset = 0;
    mark_needs_paint();
}

void MonitorWidget::set_selected_pid(int32_t pid) noexcept {
    m_selected_pid = pid;
    mark_needs_paint();
}

void MonitorWidget::set_category(ProcessCategory cat) noexcept {
    m_active_category = cat;
    if (m_category_control) {
        m_category_control->set_selected_index(static_cast<size_t>(cat));
    }
    m_scroll_offset = 0;
    mark_needs_paint();
}

void MonitorWidget::set_sort(SortColumn col, bool desc) noexcept {
    m_sort_column = col;
    m_sort_descending = desc;
    mark_needs_paint();
}

void MonitorWidget::switch_tab(MonitorTab tab) noexcept {
    if (m_active_tab != tab) {
        m_active_tab = tab;
        if (m_tabbar && m_tabbar->selected_index() != static_cast<size_t>(tab)) {
            m_tabbar->set_selected_index(static_cast<size_t>(tab));
        }
        mark_needs_layout();
        mark_needs_paint();
    }
}

void MonitorWidget::refresh_telemetry() noexcept {
    m_latest_snapshot = ResourceMonitor::instance().collect_snapshot();
    m_services = tinexus::platform::PlatformServices::query_supervised_services();

    // 1. Post-kill verification tracking
    if (m_pending_kill_pid > 0) {
        bool still_alive = false;
        for (const auto& p : m_latest_snapshot.processes) {
            if (p.pid == m_pending_kill_pid) {
                still_alive = true;
                break;
            }
        }
        if (!still_alive) {
            m_action_feedback = "Process PID " + std::to_string(m_pending_kill_pid) + " terminated.";
            m_pending_kill_pid = -1;
        } else {
            auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_pending_kill_time).count();
            if (elapsed >= 3.0) {
                m_action_feedback = "PID " + std::to_string(m_pending_kill_pid) + " not responding. Force Quit recommended.";
            }
        }
    }

    // 2. CPU Graph Update
    float cpu_pct = m_latest_snapshot.cpu_aggregate_usage_percent;
    m_cpu_graph->push_value(0, cpu_pct);
    std::ostringstream cpu_oss;
    cpu_oss << std::fixed << std::setprecision(1) << cpu_pct << "% ("
            << m_latest_snapshot.cpu_cores.size() << " Cores)";
    m_cpu_graph->set_current_value_string(cpu_oss.str());

    // 3. Memory Graph Update
    const auto& mem = m_latest_snapshot.memory;
    float mem_pct = (mem.total_ram_bytes > 0)
                        ? (static_cast<float>(mem.used_ram_bytes) / static_cast<float>(mem.total_ram_bytes)) * 100.0f
                        : 0.0f;
    m_memory_graph->push_value(0, mem_pct);
    double used_gb = static_cast<double>(mem.used_ram_bytes) / (1024.0 * 1024.0 * 1024.0);
    double total_gb = static_cast<double>(mem.total_ram_bytes) / (1024.0 * 1024.0 * 1024.0);
    std::ostringstream mem_oss;
    mem_oss << std::fixed << std::setprecision(1) << used_gb << " / " << total_gb << " GB ("
            << std::setprecision(0) << mem_pct << "%)";
    m_memory_graph->set_current_value_string(mem_oss.str());

    // 4. Disk Graph Update
    const auto& disk = m_latest_snapshot.disk;
    double total_io_kb = static_cast<double>(disk.read_bytes_sec + disk.write_bytes_sec) / 1024.0;
    float disk_max = std::max(500.0f, static_cast<float>(total_io_kb * 1.5));
    m_disk_graph->set_series_range(0, 0.0f, disk_max);
    m_disk_graph->push_value(0, static_cast<float>(total_io_kb));

    std::ostringstream disk_oss;
    if (total_io_kb >= 1024.0) {
        disk_oss << std::fixed << std::setprecision(1) << (total_io_kb / 1024.0) << " MB/s";
    } else {
        disk_oss << std::fixed << std::setprecision(0) << total_io_kb << " KB/s";
    }
    m_disk_graph->set_current_value_string(disk_oss.str());

    // 5. Network Graph Update
    const auto& net = m_latest_snapshot.network;
    double total_net_kb = static_cast<double>(net.rx_bytes_sec + net.tx_bytes_sec) / 1024.0;
    float net_max = std::max(500.0f, static_cast<float>(total_net_kb * 1.5));
    m_network_graph->set_series_range(0, 0.0f, net_max);
    m_network_graph->push_value(0, static_cast<float>(total_net_kb));

    std::ostringstream net_oss;
    if (total_net_kb >= 1024.0) {
        net_oss << std::fixed << std::setprecision(1) << (total_net_kb / 1024.0) << " MB/s ("
                << net.active_interface << ")";
    } else {
        net_oss << std::fixed << std::setprecision(0) << total_net_kb << " KB/s ("
                << net.active_interface << ")";
    }
    m_network_graph->set_current_value_string(net_oss.str());

    mark_needs_paint();
}

std::vector<ProcessInfo> MonitorWidget::get_filtered_and_sorted_processes() const {
    std::vector<ProcessInfo> filtered;
    filtered.reserve(m_latest_snapshot.processes.size());

    std::string query_lower = m_search_query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

    for (const auto& proc : m_latest_snapshot.processes) {
        // Category filtering
        if (m_active_category == ProcessCategory::MyProcesses && proc.uid != m_current_uid) {
            continue;
        }
        if (m_active_category == ProcessCategory::System && !proc.is_system_daemon) {
            continue;
        }

        // Substring search filtering
        if (!query_lower.empty()) {
            std::string name_lower = proc.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::string pid_str = std::to_string(proc.pid);
            if (name_lower.find(query_lower) == std::string::npos &&
                pid_str.find(query_lower) == std::string::npos) {
                continue;
            }
        }

        filtered.push_back(proc);
    }

    // Strict weak ordering sorting
    std::sort(filtered.begin(), filtered.end(), [this](const ProcessInfo& a, const ProcessInfo& b) {
        if (m_sort_descending) {
            switch (m_sort_column) {
                case SortColumn::CPU:    return a.cpu_percent > b.cpu_percent;
                case SortColumn::Memory: return a.rss_bytes > b.rss_bytes;
                case SortColumn::PID:    return a.pid > b.pid;
                case SortColumn::Name:   return a.name > b.name;
                case SortColumn::User:   return a.user > b.user;
                case SortColumn::DiskIO: return (a.read_bytes_sec + a.write_bytes_sec) > (b.read_bytes_sec + b.write_bytes_sec);
                case SortColumn::State:  return a.state_str > b.state_str;
            }
        } else {
            switch (m_sort_column) {
                case SortColumn::CPU:    return a.cpu_percent < b.cpu_percent;
                case SortColumn::Memory: return a.rss_bytes < b.rss_bytes;
                case SortColumn::PID:    return a.pid < b.pid;
                case SortColumn::Name:   return a.name < b.name;
                case SortColumn::User:   return a.user < b.user;
                case SortColumn::DiskIO: return (a.read_bytes_sec + a.write_bytes_sec) < (b.read_bytes_sec + b.write_bytes_sec);
                case SortColumn::State:  return a.state_str < b.state_str;
            }
        }
        return false;
    });

    return filtered;
}

void MonitorWidget::trigger_end_process(bool force) {
    if (m_selected_pid <= 0) return;

    for (const auto& proc : m_latest_snapshot.processes) {
        if (proc.pid == m_selected_pid) {
            m_target_kill_pid = proc.pid;
            m_target_kill_name = proc.name;
            m_kill_force = force;
            m_target_other_user = (proc.uid != m_current_uid);

            // Authoritative cross-check against actual PIDs tracked by PlatformServices
            // (Same source of truth used in the Services tab)
            bool is_protected = (proc.pid == 1);
            if (!is_protected) {
                // Check currently tracked supervised services in MonitorWidget
                for (const auto& svc : m_services) {
                    if (svc.is_protected && svc.active && svc.pid > 0 && svc.pid == proc.pid) {
                        is_protected = true;
                        break;
                    }
                }
            }
            if (!is_protected) {
                // Fresh query to PlatformServices to guarantee zero race conditions
                is_protected = tinexus::platform::PlatformServices::is_protected_pid(proc.pid);
            }

            m_target_is_protected = is_protected;
            m_show_kill_modal = true;
            mark_needs_paint();
            return;
        }
    }
}

Size MonitorWidget::measure_override(const Constraints& c) noexcept {
    return Size(c.max_width, c.max_height);
}

void MonitorWidget::layout_override(const Rect& f) noexcept {
    // 1. Top Tab Bar
    const double tab_w = 420.0;
    const double tab_h = 32.0;
    const double tab_x = f.x() + (f.width() - tab_w) * 0.5;
    const double tab_y = f.y() + 12.0;
    if (m_tabbar) {
        m_tabbar->layout(Rect(tab_x, tab_y, tab_w, tab_h));
    }

    // 2. Performance Tab Layout
    if (m_active_tab == MonitorTab::Performance) {
        const double pad = 24.0;
        const double gap = 16.0;
        const double top_y = f.y() + 56.0;
        const double avail_w = f.width() - pad * 2.0;
        const double col_w = (avail_w - gap) * 0.5;
        const double chart_h = 160.0;

        if (m_cpu_graph) m_cpu_graph->layout(Rect(f.x() + pad, top_y, col_w, chart_h));
        if (m_disk_graph) m_disk_graph->layout(Rect(f.x() + pad, top_y + chart_h + gap, col_w, chart_h));
        if (m_memory_graph) m_memory_graph->layout(Rect(f.x() + pad + col_w + gap, top_y, col_w, chart_h));
        if (m_network_graph) m_network_graph->layout(Rect(f.x() + pad + col_w + gap, top_y + chart_h + gap, col_w, chart_h));
    }

    // 3. Processes Tab Layout (Search Input + Category Control)
    if (m_active_tab == MonitorTab::Processes) {
        const double pad = 24.0;
        const double tb_y = f.y() + 56.0 + 48.0;
        if (m_search_input) {
            m_search_input->layout(Rect(f.x() + pad, tb_y, 220.0, 30.0));
        }
        if (m_category_control) {
            m_category_control->layout(Rect(f.x() + pad + 232.0, tb_y, 280.0, 30.0));
        }
    }
}

void MonitorWidget::paint_override(Painter& p) const noexcept {
    const Rect f = frame();
    p.fill_rect(f, colors::BG_DARK);

    // Tab bar paint
    if (m_tabbar) {
        m_tabbar->paint(p);
    }

    const Rect content_rect(f.x() + 24.0, f.y() + 56.0, f.width() - 48.0, f.height() - 72.0);

    switch (m_active_tab) {
        case MonitorTab::Performance:
            paint_performance_tab(p, content_rect);
            break;
        case MonitorTab::Services:
            paint_services_tab(p, content_rect);
            break;
        case MonitorTab::Processes:
            paint_processes_tab(p, content_rect);
            break;
    }

    // Render Safety-Critical Confirmation Modal on top
    if (m_show_kill_modal) {
        paint_kill_confirmation_modal(p);
    }
}

void MonitorWidget::paint_performance_tab(Painter& p, const Rect& area) const noexcept {
    if (m_cpu_graph) m_cpu_graph->paint(p);
    if (m_memory_graph) m_memory_graph->paint(p);
    if (m_disk_graph) m_disk_graph->paint(p);
    if (m_network_graph) m_network_graph->paint(p);

    const double gap = 16.0;
    const double col_w = (area.width() - gap) * 0.5;
    const double card_y = area.y() + 160.0 * 2.0 + gap * 2.0;
    const double card_h = 100.0;

    // Power Card
    Rect power_card(area.x(), card_y, col_w, card_h);
    p.fill_rounded_rect(power_card, 8.0, colors::CARD_BG);
    p.fill_rounded_rect(Rect(power_card.x(), power_card.y(), power_card.width(), 1.0), 0.0, colors::CARD_BORDER);

    p.draw_text(Point(static_cast<Coordinate>(power_card.x() + 16.0), static_cast<Coordinate>(power_card.y() + 14.0)),
                "Power & Battery Telemetry", colors::TXT_PRI, 13.0, true);

    const auto& pwr = m_latest_snapshot.power;
    std::string pwr_str = pwr.has_battery
                              ? (pwr.power_status + " (" + std::to_string(pwr.battery_percent) + "%)")
                              : "AC Power Connected (100%)";
    p.draw_text(Point(static_cast<Coordinate>(power_card.x() + 16.0), static_cast<Coordinate>(power_card.y() + 40.0)),
                pwr_str, colors::TXT_SEC, 12.0, false);

    std::ostringstream w_oss;
    if (pwr.power_watts > 0.0f) {
        w_oss << "Real Power Draw: " << std::fixed << std::setprecision(1) << pwr.power_watts << " W";
    } else {
        w_oss << "Plugged in — minimal power draw";
    }
    p.draw_text(Point(static_cast<Coordinate>(power_card.x() + 16.0), static_cast<Coordinate>(power_card.y() + 64.0)),
                w_oss.str(), colors::TXT_DIM, 11.5, false);

    auto energy_ext = p.measure_text("Energy Impact: Low (estimated)", 10.5);
    p.draw_text(Point(static_cast<Coordinate>(power_card.x() + power_card.width() - energy_ext.width - 16.0),
                      static_cast<Coordinate>(power_card.y() + 14.0)),
                "Energy Impact: Low (estimated)", colors::TXT_DIM, 10.5, false);

    // GPU Card
    Rect gpu_card(area.x() + col_w + gap, card_y, col_w, card_h);
    p.fill_rounded_rect(gpu_card, 8.0, colors::CARD_BG);
    p.fill_rounded_rect(Rect(gpu_card.x(), gpu_card.y(), gpu_card.width(), 1.0), 0.0, colors::CARD_BORDER);

    p.draw_text(Point(static_cast<Coordinate>(gpu_card.x() + 16.0), static_cast<Coordinate>(gpu_card.y() + 14.0)),
                "GPU Graphics Engine", colors::TXT_PRI, 13.0, true);

    const auto& gpu = m_latest_snapshot.gpu;
    std::string gpu_title = gpu.vendor + " " + gpu.device_name + " (" + gpu.driver + ")";
    p.draw_text(Point(static_cast<Coordinate>(gpu_card.x() + 16.0), static_cast<Coordinate>(gpu_card.y() + 40.0)),
                gpu_title, colors::TXT_SEC, 12.0, false);

    p.draw_text(Point(static_cast<Coordinate>(gpu_card.x() + 16.0), static_cast<Coordinate>(gpu_card.y() + 64.0)),
                gpu.telemetry_status, colors::TXT_DIM, 11.5, false);
}

void MonitorWidget::paint_services_tab(Painter& p, const Rect& area) const noexcept {
    p.draw_text(Point(static_cast<Coordinate>(area.x()), static_cast<Coordinate>(area.y() + 4.0)),
                "Platform Supervision Tree (tinexus-serviced)", colors::TXT_PRI, 15.0, true);
    p.draw_text(Point(static_cast<Coordinate>(area.x()), static_cast<Coordinate>(area.y() + 24.0)),
                "Real-time health and supervisor state of core Wayland desktop daemons", colors::TXT_SEC, 12.0);

    if (!m_action_feedback.empty()) {
        auto fb_ext = p.measure_text(m_action_feedback, 12.0, true);
        p.draw_text(Point(static_cast<Coordinate>(area.x() + area.width() - fb_ext.width), static_cast<Coordinate>(area.y() + 4.0)),
                    m_action_feedback, colors::ACCENT_BLUE, 12.0, true);
    }

    double card_y = area.y() + 48.0;
    const double card_h = 58.0;
    const double gap = 8.0;

    for (const auto& svc : m_services) {
        Rect s_card(area.x(), card_y, area.width(), card_h);
        p.fill_rounded_rect(s_card, 6.0, colors::CARD_BG);
        p.fill_rounded_rect(Rect(s_card.x(), s_card.y(), s_card.width(), 1.0), 0.0, colors::CARD_BORDER);

        Point dot_center(static_cast<Coordinate>(s_card.x() + 24.0),
                         static_cast<Coordinate>(s_card.y() + card_h * 0.5));
        if (svc.active) {
            p.fill_circle(dot_center, 8.5, Color(34, 197, 94, 45));
            p.fill_circle(dot_center, 5.0, colors::STATUS_ACTIVE);
        } else {
            p.fill_circle(dot_center, 8.0, Color(245, 158, 11, 40));
            p.fill_circle(dot_center, 5.0, colors::STATUS_STANDBY);
        }

        p.draw_text(Point(static_cast<Coordinate>(s_card.x() + 46.0), static_cast<Coordinate>(s_card.y() + 13.0)),
                    svc.name, colors::TXT_PRI, 13.5, true);
        p.draw_text(Point(static_cast<Coordinate>(s_card.x() + 46.0), static_cast<Coordinate>(s_card.y() + 33.0)),
                    svc.role, colors::TXT_DIM, 11.5);

        Color badge_bg = svc.active ? colors::BADGE_ACTIVE_BG : colors::BADGE_STANDBY_BG;
        Color badge_txt = svc.active ? colors::STATUS_ACTIVE : colors::STATUS_STANDBY;
        const double badge_w = 114.0;
        const double badge_h = 24.0;
        const double badge_x = s_card.x() + s_card.width() - 276.0;
        const double badge_y = s_card.y() + (card_h - badge_h) * 0.5;

        p.fill_rounded_rect(Rect(badge_x, badge_y, badge_w, badge_h), 4.0, badge_bg);
        auto badge_ext = p.measure_text(svc.status_str, 11.0, true);
        double badge_text_x = badge_x + (badge_w - badge_ext.width) * 0.5;
        double badge_text_y = badge_y + (badge_h - 11.0) * 0.5 - 1.0;
        p.draw_text(Point(static_cast<Coordinate>(badge_text_x), static_cast<Coordinate>(badge_text_y)),
                    svc.status_str, badge_txt, 11.0, true);

        const double btn_w = 132.0;
        const double btn_h = 28.0;
        const double btn_x = s_card.x() + s_card.width() - btn_w - 16.0;
        const double btn_y = s_card.y() + (card_h - btn_h) * 0.5;

        if (svc.is_protected) {
            p.fill_rounded_rect(Rect(btn_x, btn_y, btn_w, btn_h), 5.0, colors::PROTECTED_BG);
            p.fill_rounded_rect(Rect(btn_x, btn_y, btn_w, 1.0), 0.0, colors::PROTECTED_BORDER);
            auto p_ext = p.measure_text("🔒 Protected Session", 10.5, true);
            double p_text_x = btn_x + (btn_w - p_ext.width) * 0.5;
            double p_text_y = btn_y + (btn_h - 10.5) * 0.5 - 1.0;
            p.draw_text(Point(static_cast<Coordinate>(p_text_x), static_cast<Coordinate>(p_text_y)),
                        "🔒 Protected Session", colors::PROTECTED_TXT, 10.5, true);
        } else {
            p.fill_rounded_rect(Rect(btn_x, btn_y, btn_w, btn_h), 5.0, colors::GHOST_BTN_BG);
            p.fill_rounded_rect(Rect(btn_x, btn_y, btn_w, 1.0), 0.0, colors::GHOST_BTN_BORDER);
            auto btn_ext = p.measure_text("Restart Service", 11.5, true);
            double b_text_x = btn_x + (btn_w - btn_ext.width) * 0.5;
            double b_text_y = btn_y + (btn_h - 11.5) * 0.5 - 1.0;
            p.draw_text(Point(static_cast<Coordinate>(b_text_x), static_cast<Coordinate>(b_text_y)),
                        "Restart Service", colors::TXT_PRI, 11.5, true);
        }

        card_y += card_h + gap;
    }
}

void MonitorWidget::paint_processes_tab(Painter& p, const Rect& area) const noexcept {
    auto procs = get_filtered_and_sorted_processes();

    // 1. Header Title & Counts
    p.draw_text(Point(static_cast<Coordinate>(area.x()), static_cast<Coordinate>(area.y() + 4.0)),
                "Process Management", colors::TXT_PRI, 15.0, true);

    std::string count_str = "Showing " + std::to_string(procs.size()) + " of " +
                            std::to_string(m_latest_snapshot.processes.size()) + " Processes";
    p.draw_text(Point(static_cast<Coordinate>(area.x()), static_cast<Coordinate>(area.y() + 24.0)),
                count_str, colors::TXT_SEC, 12.0);

    // Feedback notification in header
    if (!m_action_feedback.empty()) {
        auto fb_ext = p.measure_text(m_action_feedback, 12.0, true);
        p.draw_text(Point(static_cast<Coordinate>(area.x() + area.width() - fb_ext.width), static_cast<Coordinate>(area.y() + 4.0)),
                    m_action_feedback, colors::ACCENT_AMBER, 12.0, true);
    }

    // 2. Toolbar: Search Input + Category SegmentedControl + Action Buttons
    if (m_search_input) m_search_input->paint(p);
    if (m_category_control) m_category_control->paint(p);

    const double tb_y = area.y() + 48.0;
    const double tb_end_x = area.x() + area.width();

    // End Process button (SIGTERM)
    Rect btn_end_rect(tb_end_x - 216.0, tb_y, 104.0, 30.0);
    bool has_selection = (m_selected_pid > 0);
    Color end_bg = has_selection ? Color(245, 158, 11, 20) : Color(255, 255, 255, 6);
    Color end_border = has_selection ? Color(245, 158, 11, 80) : Color(255, 255, 255, 18);
    Color end_txt = has_selection ? Color(251, 191, 36, 255) : colors::TXT_DIM;

    p.fill_rounded_rect(btn_end_rect, 5.0, end_bg);
    p.fill_rounded_rect(Rect(btn_end_rect.x(), btn_end_rect.y(), btn_end_rect.width(), 1.0), 0.0, end_border);
    auto end_ext = p.measure_text("End Process", 11.5, true);
    p.draw_text(Point(static_cast<Coordinate>(btn_end_rect.x() + (btn_end_rect.width() - end_ext.width) * 0.5),
                      static_cast<Coordinate>(btn_end_rect.y() + (btn_end_rect.height() - 11.5) * 0.5 - 1.0)),
                "End Process", end_txt, 11.5, true);

    // Force Quit button (SIGKILL)
    Rect btn_kill_rect(tb_end_x - 104.0, tb_y, 104.0, 30.0);
    Color kill_bg = has_selection ? Color(239, 68, 68, 20) : Color(255, 255, 255, 6);
    Color kill_border = has_selection ? Color(239, 68, 68, 80) : Color(255, 255, 255, 18);
    Color kill_txt = has_selection ? Color(248, 113, 113, 255) : colors::TXT_DIM;

    p.fill_rounded_rect(btn_kill_rect, 5.0, kill_bg);
    p.fill_rounded_rect(Rect(btn_kill_rect.x(), btn_kill_rect.y(), btn_kill_rect.width(), 1.0), 0.0, kill_border);
    auto kill_ext = p.measure_text("Force Quit", 11.5, true);
    p.draw_text(Point(static_cast<Coordinate>(btn_kill_rect.x() + (btn_kill_rect.width() - kill_ext.width) * 0.5),
                      static_cast<Coordinate>(btn_kill_rect.y() + (btn_kill_rect.height() - 11.5) * 0.5 - 1.0)),
                "Force Quit", kill_txt, 11.5, true);

    // 3. Table Container Card
    Rect table_card(area.x(), tb_y + 38.0, area.width(), area.height() - 88.0);
    p.fill_rounded_rect(table_card, 8.0, colors::CARD_BG);
    p.fill_rounded_rect(Rect(table_card.x(), table_card.y(), table_card.width(), 1.0), 0.0, colors::CARD_BORDER);

    // Column X positions (responsive to table width)
    const double table_w    = table_card.width();
    const double col_name   = table_card.x() + 16.0;
    const double col_pid    = table_card.x() + std::max(200.0, table_w * 0.28);
    const double col_user   = table_card.x() + std::max(270.0, table_w * 0.38);
    const double col_cpu    = table_card.x() + std::max(390.0, table_w * 0.52);
    const double col_mem    = table_card.x() + std::max(470.0, table_w * 0.64);
    const double col_io     = table_card.x() + std::max(560.0, table_w * 0.77);
    const double col_state  = table_card.x() + std::max(660.0, table_w * 0.89);

    // Header strip
    Rect th_rect(table_card.x() + 1.0, table_card.y() + 1.0, table_card.width() - 2.0, 32.0);
    p.fill_rounded_rect(th_rect, 6.0, Color(28, 28, 38, 255));
    p.draw_line(Point(static_cast<Coordinate>(th_rect.x()), static_cast<Coordinate>(th_rect.y() + th_rect.height())),
                Point(static_cast<Coordinate>(th_rect.x() + th_rect.width()), static_cast<Coordinate>(th_rect.y() + th_rect.height())),
                1.0, colors::CARD_BORDER);

    const double th_text_y = th_rect.y() + 9.0;
    auto draw_header_col = [&](const std::string& label, double x_pos, SortColumn col) {
        std::string text = label;
        if (m_sort_column == col) {
            text += m_sort_descending ? " ▼" : " ▲";
        }
        Color col_color = (m_sort_column == col) ? colors::ACCENT_BLUE : colors::TXT_SEC;
        p.draw_text(Point(static_cast<Coordinate>(x_pos), static_cast<Coordinate>(th_text_y)), text, col_color, 11.5, true);
    };

    draw_header_col("Process Name", col_name, SortColumn::Name);
    draw_header_col("PID", col_pid, SortColumn::PID);
    draw_header_col("User", col_user, SortColumn::User);
    draw_header_col("CPU %", col_cpu, SortColumn::CPU);
    draw_header_col("Memory", col_mem, SortColumn::Memory);
    draw_header_col("Disk I/O", col_io, SortColumn::DiskIO);
    draw_header_col("State", col_state, SortColumn::State);

    // Rows rendering
    double row_y = th_rect.y() + th_rect.height() + 1.0;
    const double row_h = 32.0;
    size_t visible_rows_count = static_cast<size_t>((table_card.height() - 36.0) / row_h);
    size_t start_idx = std::min(m_scroll_offset, procs.empty() ? 0 : procs.size() - 1);
    size_t end_idx = std::min(start_idx + visible_rows_count, procs.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        const auto& proc = procs[i];
        Rect r(table_card.x() + 2.0, row_y, table_card.width() - 4.0, row_h);

        bool is_selected = (proc.pid == m_selected_pid);

        if (is_selected) {
            p.fill_rounded_rect(r, 4.0, colors::ROW_SELECTED_BG);
            p.fill_rounded_rect(Rect(r.x(), r.y(), r.width(), 1.0), 0.0, colors::ROW_SELECTED_BDR);
        } else if (i % 2 == 1) {
            p.fill_rect(r, Color(255, 255, 255, 5));
        }

        // Row bottom divider line
        p.draw_line(Point(static_cast<Coordinate>(r.x() + 12.0), static_cast<Coordinate>(r.y() + row_h)),
                    Point(static_cast<Coordinate>(r.x() + r.width() - 12.0), static_cast<Coordinate>(r.y() + row_h)),
                    1.0, Color(255, 255, 255, 8));

        const double row_text_y = r.y() + 9.0;
        p.draw_text(Point(static_cast<Coordinate>(col_name), static_cast<Coordinate>(row_text_y)), proc.name, colors::TXT_PRI, 12.0);
        p.draw_text(Point(static_cast<Coordinate>(col_pid), static_cast<Coordinate>(row_text_y)), std::to_string(proc.pid), colors::TXT_SEC, 11.5);
        p.draw_text(Point(static_cast<Coordinate>(col_user), static_cast<Coordinate>(row_text_y)), proc.user, colors::TXT_SEC, 11.5);

        // CPU %
        std::ostringstream cpu_oss;
        cpu_oss << std::fixed << std::setprecision(1) << proc.cpu_percent << "%";
        Color cpu_col = proc.cpu_percent > 10.0f ? colors::ACCENT_AMBER : colors::TXT_SEC;
        p.draw_text(Point(static_cast<Coordinate>(col_cpu), static_cast<Coordinate>(row_text_y)), cpu_oss.str(), cpu_col, 11.5);

        // Memory (RSS)
        double rss_mb = static_cast<double>(proc.rss_bytes) / (1024.0 * 1024.0);
        std::ostringstream mem_oss;
        mem_oss << std::fixed << std::setprecision(1) << rss_mb << " MB";
        p.draw_text(Point(static_cast<Coordinate>(col_mem), static_cast<Coordinate>(row_text_y)), mem_oss.str(), colors::TXT_SEC, 11.5);

        // Disk I/O Throughput (read + write, or "-" if EACCES)
        std::string io_str;
        if (!proc.has_io_permission) {
            io_str = "-";
        } else {
            uint64_t total_io = proc.read_bytes_sec + proc.write_bytes_sec;
            if (total_io >= 1024 * 1024) {
                std::ostringstream io_oss;
                io_oss << std::fixed << std::setprecision(1) << (static_cast<double>(total_io) / (1024.0 * 1024.0)) << " MB/s";
                io_str = io_oss.str();
            } else if (total_io >= 1024) {
                io_str = std::to_string(total_io / 1024) + " KB/s";
            } else {
                io_str = "0 KB/s";
            }
        }
        p.draw_text(Point(static_cast<Coordinate>(col_io), static_cast<Coordinate>(row_text_y)), io_str, colors::TXT_DIM, 11.5);

        // State string
        p.draw_text(Point(static_cast<Coordinate>(col_state), static_cast<Coordinate>(row_text_y)), proc.state_str, colors::TXT_SEC, 11.5);

        row_y += row_h;
    }
}

void MonitorWidget::paint_kill_confirmation_modal(Painter& p) const noexcept {
    const Rect f = frame();

    // 1. Semi-transparent backdrop dimming
    p.fill_rect(f, Color(0, 0, 0, 160));

    // 2. Dialog box in center
    const double dw = 480.0;
    const double dh = 240.0;
    const double dx = f.x() + (f.width() - dw) * 0.5;
    const double dy = f.y() + (f.height() - dh) * 0.5;
    Rect dlg_rect(dx, dy, dw, dh);

    p.fill_rounded_rect(dlg_rect, 10.0, Color(24, 24, 34, 255));
    p.fill_rounded_rect(Rect(dlg_rect.x(), dlg_rect.y(), dlg_rect.width(), 1.0), 0.0, Color(255, 255, 255, 30));

    // Title
    std::string title = m_kill_force ? "Force Quit Process?" : "End Process?";
    Color title_col = m_kill_force ? Color(239, 68, 68, 255) : Color(245, 158, 11, 255);
    p.draw_text(Point(static_cast<Coordinate>(dx + 24.0), static_cast<Coordinate>(dy + 18.0)), title, title_col, 16.0, true);

    // Message
    std::string msg = "Do you want to terminate '" + m_target_kill_name + "' (PID " + std::to_string(m_target_kill_pid) + ")?";
    p.draw_text(Point(static_cast<Coordinate>(dx + 24.0), static_cast<Coordinate>(dy + 48.0)), msg, colors::TXT_PRI, 13.0, true);

    std::string desc = m_kill_force
                           ? "SIGKILL will terminate the process immediately. Unsaved data will be permanently lost."
                           : "SIGTERM requests the application to close cleanly and save its work.";
    p.draw_text(Point(static_cast<Coordinate>(dx + 24.0), static_cast<Coordinate>(dy + 72.0)), desc, colors::TXT_SEC, 11.5);

    // High-Contrast Warning Banner (if protected, or PID 1, or root/other user)
    Rect warn_rect(dx + 24.0, dy + 104.0, dw - 48.0, 48.0);
    if (m_target_is_protected) {
        p.fill_rounded_rect(warn_rect, 6.0, colors::WARNING_BANNER);
        p.fill_rounded_rect(Rect(warn_rect.x(), warn_rect.y(), warn_rect.width(), 1.0), 0.0, Color(239, 68, 68, 120));
        p.draw_text(Point(static_cast<Coordinate>(warn_rect.x() + 12.0), static_cast<Coordinate>(warn_rect.y() + 10.0)),
                    "⚠️ CRITICAL SYSTEM DAEMON", Color(248, 113, 113, 255), 11.5, true);
        p.draw_text(Point(static_cast<Coordinate>(warn_rect.x() + 12.0), static_cast<Coordinate>(warn_rect.y() + 28.0)),
                    "Terminating this component will immediately crash your desktop session.", Color(254, 202, 202, 255), 11.0);
    } else if (m_target_other_user) {
        p.fill_rounded_rect(warn_rect, 6.0, Color(245, 158, 11, 30));
        p.fill_rounded_rect(Rect(warn_rect.x(), warn_rect.y(), warn_rect.width(), 1.0), 0.0, Color(245, 158, 11, 80));
        p.draw_text(Point(static_cast<Coordinate>(warn_rect.x() + 12.0), static_cast<Coordinate>(warn_rect.y() + 10.0)),
                    "⚠️ SYSTEM / ROOT PRIVILEGE", Color(251, 191, 36, 255), 11.5, true);
        p.draw_text(Point(static_cast<Coordinate>(warn_rect.x() + 12.0), static_cast<Coordinate>(warn_rect.y() + 28.0)),
                    "This process is owned by another user. Signal may be rejected without root.", Color(253, 230, 138, 255), 11.0);
    }

    // Bottom Action Buttons
    // 1. Cancel button
    Rect cancel_rect(dx + dw - 240.0, dy + dh - 48.0, 96.0, 32.0);
    p.fill_rounded_rect(cancel_rect, 5.0, Color(255, 255, 255, 12));
    p.fill_rounded_rect(Rect(cancel_rect.x(), cancel_rect.y(), cancel_rect.width(), 1.0), 0.0, Color(255, 255, 255, 30));
    auto c_ext = p.measure_text("Cancel", 12.0, true);
    p.draw_text(Point(static_cast<Coordinate>(cancel_rect.x() + (cancel_rect.width() - c_ext.width) * 0.5),
                      static_cast<Coordinate>(cancel_rect.y() + (cancel_rect.height() - 12.0) * 0.5 - 1.0)),
                "Cancel", colors::TXT_PRI, 12.0, true);

    // 2. Action button (or disabled if protected)
    Rect act_rect(dx + dw - 132.0, dy + dh - 48.0, 108.0, 32.0);
    if (m_target_is_protected) {
        p.fill_rounded_rect(act_rect, 5.0, Color(255, 255, 255, 8));
        auto a_ext = p.measure_text("Protected", 11.5, true);
        p.draw_text(Point(static_cast<Coordinate>(act_rect.x() + (act_rect.width() - a_ext.width) * 0.5),
                          static_cast<Coordinate>(act_rect.y() + (act_rect.height() - 11.5) * 0.5 - 1.0)),
                    "Protected", colors::TXT_DIM, 11.5, true);
    } else {
        Color btn_bg = m_kill_force ? colors::DANGER_BG : Color(245, 158, 11, 230);
        std::string btn_label = m_kill_force ? "Force Quit" : "End Process";
        p.fill_rounded_rect(act_rect, 5.0, btn_bg);
        p.fill_rounded_rect(Rect(act_rect.x(), act_rect.y(), act_rect.width(), 1.0), 0.0, Color(255, 255, 255, 60));
        auto a_ext = p.measure_text(btn_label, 12.0, true);
        p.draw_text(Point(static_cast<Coordinate>(act_rect.x() + (act_rect.width() - a_ext.width) * 0.5),
                          static_cast<Coordinate>(act_rect.y() + (act_rect.height() - 12.0) * 0.5 - 1.0)),
                    btn_label, Color(255, 255, 255, 255), 12.0, true);
    }
}

bool MonitorWidget::handle_event(const Event& event) noexcept {
    // 1. Safety-Critical Modal Event Interception (Cannot be bypassed)
    if (m_show_kill_modal) {
        if (event.type == EventType::PointerButtonPress) {
            const Rect f = frame();
            const double dw = 480.0;
            const double dh = 240.0;
            const double dx = f.x() + (f.width() - dw) * 0.5;
            const double dy = f.y() + (f.height() - dh) * 0.5;

            Point click_pt(event.pointer.x, event.pointer.y);
            Rect cancel_rect(dx + dw - 240.0, dy + dh - 48.0, 96.0, 32.0);
            Rect act_rect(dx + dw - 132.0, dy + dh - 48.0, 108.0, 32.0);

            if (cancel_rect.contains(click_pt)) {
                m_show_kill_modal = false;
                mark_needs_paint();
                return true;
            }

            if (act_rect.contains(click_pt) && !m_target_is_protected) {
                ProcessController::terminate_process(m_target_kill_pid, m_kill_force);
                m_pending_kill_pid = m_target_kill_pid;
                m_pending_kill_time = std::chrono::steady_clock::now();
                m_show_kill_modal = false;
                refresh_telemetry();
                return true;
            }

            Rect dlg_rect(dx, dy, dw, dh);
            if (!dlg_rect.contains(click_pt)) {
                m_show_kill_modal = false;
                mark_needs_paint();
                return true;
            }
        }
        return true;
    }

    // 2. Normal Window Event Handling
    if (m_tabbar && m_tabbar->handle_event(event)) {
        return true;
    }

    if (m_active_tab == MonitorTab::Processes) {
        if (m_search_input && m_search_input->handle_event(event)) {
            return true;
        }
        if (m_category_control && m_category_control->handle_event(event)) {
            return true;
        }

        const Rect f = frame();
        const Rect content_rect(f.x() + 24.0, f.y() + 56.0, f.width() - 48.0, f.height() - 72.0);
        const double tb_y = content_rect.y() + 48.0;
        const double tb_end_x = content_rect.x() + content_rect.width();

        Rect btn_end_rect(tb_end_x - 216.0, tb_y, 104.0, 30.0);
        Rect btn_kill_rect(tb_end_x - 104.0, tb_y, 104.0, 30.0);

        Rect table_card(content_rect.x(), tb_y + 38.0, content_rect.width(), content_rect.height() - 88.0);
        Rect th_rect(table_card.x() + 1.0, table_card.y() + 1.0, table_card.width() - 2.0, 32.0);

        // Click events
        if (event.type == EventType::PointerButtonPress) {
            Point click_pt(event.pointer.x, event.pointer.y);

            // Toolbar action buttons
            if (btn_end_rect.contains(click_pt)) {
                trigger_end_process(false);
                return true;
            }
            if (btn_kill_rect.contains(click_pt)) {
                trigger_end_process(true);
                return true;
            }

            // Click on Header to Sort Columns
            if (th_rect.contains(click_pt)) {
                double rel_x = click_pt.x - table_card.x();
                SortColumn clicked_col = SortColumn::Name;
                if (rel_x < 224.0) clicked_col = SortColumn::Name;
                else if (rel_x < 300.0) clicked_col = SortColumn::PID;
                else if (rel_x < 396.0) clicked_col = SortColumn::User;
                else if (rel_x < 480.0) clicked_col = SortColumn::CPU;
                else if (rel_x < 580.0) clicked_col = SortColumn::Memory;
                else if (rel_x < 690.0) clicked_col = SortColumn::DiskIO;
                else clicked_col = SortColumn::State;

                if (m_sort_column == clicked_col) {
                    m_sort_descending = !m_sort_descending;
                } else {
                    m_sort_column = clicked_col;
                    m_sort_descending = true;
                }
                mark_needs_paint();
                return true;
            }

            // Click on Table Row to Select Process
            if (table_card.contains(click_pt) && !th_rect.contains(click_pt)) {
                double rows_start_y = th_rect.y() + th_rect.height() + 1.0;
                double offset_y = click_pt.y - rows_start_y;
                if (offset_y >= 0) {
                    size_t row_idx = static_cast<size_t>(offset_y / 32.0);
                    auto procs = get_filtered_and_sorted_processes();
                    size_t actual_idx = m_scroll_offset + row_idx;
                    if (actual_idx < procs.size()) {
                        m_selected_pid = procs[actual_idx].pid;
                        mark_needs_paint();
                        return true;
                    }
                }
            }
        }

        // Mouse Wheel Scroll in Table
        if (event.type == EventType::PointerScroll) {
            Point pt(event.pointer.x, event.pointer.y);
            if (table_card.contains(pt)) {
                auto procs = get_filtered_and_sorted_processes();
                if (event.pointer.scroll_delta_y > 0 && m_scroll_offset > 0) {
                    m_scroll_offset = (m_scroll_offset >= 3) ? (m_scroll_offset - 3) : 0;
                    mark_needs_paint();
                    return true;
                } else if (event.pointer.scroll_delta_y < 0) {
                    if (m_scroll_offset + 5 < procs.size()) {
                        m_scroll_offset += 3;
                        mark_needs_paint();
                        return true;
                    }
                }
            }
        }
    }

    return Widget::handle_event(event);
}

} // namespace tinexus::monitor
