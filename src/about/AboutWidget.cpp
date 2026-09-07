#include "AboutWidget.hpp"
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <common/logger.hpp>
#include <common/TinexusLogo.hpp>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace tinexus::about {

namespace colors {
    constexpr txui::Color BG_WINDOW    { 24,  24,  28, 255}; // dark frosted background
    constexpr txui::Color TAB_BG       { 36,  36,  42, 255}; // tab pill background
    constexpr txui::Color TAB_ACTIVE   { 58,  62,  74, 255}; // active tab highlight
    constexpr txui::Color ACCENT_BLUE  { 59, 130, 246, 255}; // #3b82f6
    constexpr txui::Color ACCENT_CYAN  { 56, 189, 248, 255}; // #38bdf8
    constexpr txui::Color ACCENT_GREEN { 34, 197,  94, 255}; // #22c55e
    constexpr txui::Color TEXT_PRI     {242, 242, 248, 255};
    constexpr txui::Color TEXT_SEC     {158, 164, 180, 255};
    constexpr txui::Color TEXT_DIM     {112, 118, 134, 255};
    constexpr txui::Color BTN_BG       { 40,  44,  56, 255};
    constexpr txui::Color BTN_HOVER    { 60,  66,  84, 255};
    constexpr txui::Color BORDER_SUBTLE{255, 255, 255,  18};
}

AboutWidget::AboutWidget() {
    read_system_info();

    // 1. Top Segmented Tabs Bar
    m_tabbar = txui::make_ref<txui::SegmentedControl>(
        std::vector<std::string>{"Overview", "Displays", "Storage", "Support", "Service"}, size_t{0});
    m_tabbar->set_on_segment_selected([this](size_t index) {
        switch_tab(static_cast<AboutTab>(index));
    });
    add_child(m_tabbar);

    // 2. Action Buttons in Overview tab
    m_btn_report = txui::make_ref<txui::Button>("System Report...");
    m_btn_report->set_style(txui::Button::Style::Standard);
    m_btn_report->set_on_click([]() {
        tinexus::log::info("[About] Launching System Monitor (System Report)");
        if (fork() == 0) {
            execlp("tinexus-monitor", "tinexus-monitor", nullptr);
            _exit(127);
        }
    });
    add_child(m_btn_report);

    m_btn_update = txui::make_ref<txui::Button>("Software Update...");
    m_btn_update->set_style(txui::Button::Style::Standard);
    m_btn_update->set_on_click([]() {
        tinexus::log::info("[About] Launching Package Manager (Software Update)");
        if (fork() == 0) {
            execlp("tinexus-pkg", "tinexus-pkg", nullptr);
            _exit(127);
        }
    });
    add_child(m_btn_update);
}

void AboutWidget::read_system_info() {
    m_os_title = "Tinexus Desktop";
    m_os_version = "Version 1.0 (Architecture Freeze - LTS)";

    // 1. Kernel & Architecture
    struct utsname un;
    if (uname(&un) == 0) {
        std::string rel = un.release;
        auto ms_pos = rel.find("-microsoft");
        if (ms_pos != std::string::npos) {
            rel = rel.substr(0, ms_pos);
        }
        m_kernel_version = std::string(un.sysname) + " " + rel + " (" + un.machine + ")";
    } else {
        m_kernel_version = "Linux 6.18.33 (x86_64)";
    }

    // 2. Memory
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        uint64_t total_mb = (si.totalram * si.mem_unit) / (1024 * 1024);
        uint64_t free_mb  = (si.freeram  * si.mem_unit) / (1024 * 1024);
        uint64_t used_mb  = (total_mb > free_mb) ? (total_mb - free_mb) : 0;
        double used_gb = static_cast<double>(used_mb) / 1024.0;
        double total_gb = static_cast<double>(total_mb) / 1024.0;
        int pct = (total_mb > 0) ? static_cast<int>((used_mb * 100) / total_mb) : 0;

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << used_gb << " GB / " << total_gb << " GB (" << pct << "% used)";
        m_mem_info = ss.str();
    } else {
        m_mem_info = "8.0 GB DDR4 Memory";
    }

    // 3. CPU
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.starts_with("model name") || line.starts_with("Hardware")) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    m_cpu_model = line.substr(colon + 2);
                    break;
                }
            }
        }
    }
    if (m_cpu_model.empty()) m_cpu_model = "Intel Core i7 Processor";

    // Clean CPU model name for luxury appearance (strip redundant trademarks)
    auto remove_all = [](std::string& s, const std::string& target) {
        size_t pos = 0;
        while ((pos = s.find(target, pos)) != std::string::npos) {
            s.erase(pos, target.length());
        }
    };
    remove_all(m_cpu_model, "(R)");
    remove_all(m_cpu_model, "(TM)");
    remove_all(m_cpu_model, "CPU ");
    while (m_cpu_model.find("  ") != std::string::npos) {
        auto pos = m_cpu_model.find("  ");
        m_cpu_model.replace(pos, 2, " ");
    }

    // 4. Compositor / Graphics
    m_comp_info = "wlroots 0.17 (Vulkan RHI Compositor)";

    // 5. Storage Specs
    m_storage_spec.root_mount = "/";
    m_storage_spec.device = "Root NVMe SSD";
    m_storage_spec.fs_type = "ext4 / OverlayFS";

    std::ifstream mounts("/proc/mounts");
    if (mounts.is_open()) {
        std::string dev, mnt, fstype;
        while (mounts >> dev >> mnt >> fstype) {
            std::string dummy;
            std::getline(mounts, dummy);
            if (mnt == "/") {
                m_storage_spec.device = dev;
                m_storage_spec.fs_type = fstype;
                break;
            }
        }
    }

    struct statvfs sv;
    if (statvfs("/", &sv) == 0) {
        uint64_t total = (sv.f_blocks * sv.f_frsize) / (1024ULL * 1024ULL * 1024ULL);
        uint64_t free  = (sv.f_bfree  * sv.f_frsize) / (1024ULL * 1024ULL * 1024ULL);
        uint64_t used  = (total > free) ? (total - free) : 0;
        if (total == 0) { total = 64; used = 16; free = 48; }
        m_storage_spec.total_gb = total;
        m_storage_spec.used_gb = used;
        m_storage_spec.free_gb = free;
        m_storage_spec.used_pct = static_cast<double>(used) / static_cast<double>(total);
    } else {
        m_storage_spec.total_gb = 64;
        m_storage_spec.used_gb = 14;
        m_storage_spec.free_gb = 50;
        m_storage_spec.used_pct = 0.22;
    }

    // 6. Display Specs
    m_display_spec.name = "Built-in Display (eDP-1)";
    m_display_spec.resolution = "1920 × 1080 (Full HD)";
    m_display_spec.refresh_rate = "60.00 Hz (Hardware VSync)";
    m_display_spec.scale = "100% (Native 1:1 Pixel Grid)";
    m_display_spec.format = "32-bit ARGB8888 (sRGB D65)";
    m_display_spec.renderer = "Vulkan RHI via wlroots (DRM Direct)";

    DIR* drm_dir = opendir("/sys/class/drm");
    if (drm_dir) {
        struct dirent* entry;
        while ((entry = readdir(drm_dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.find("card") != std::string::npos && name.find("-") != std::string::npos) {
                std::string status_path = "/sys/class/drm/" + name + "/status";
                std::ifstream sf(status_path);
                std::string status;
                if (sf >> status && status == "connected") {
                    auto dash = name.find('-');
                    m_display_spec.name = name.substr(dash + 1) + " (Connected)";
                    std::string mode_path = "/sys/class/drm/" + name + "/modes";
                    std::ifstream mf(mode_path);
                    std::string first_mode;
                    if (mf >> first_mode && !first_mode.empty()) {
                        m_display_spec.resolution = first_mode + " (Native)";
                    }
                    break;
                }
            }
        }
        closedir(drm_dir);
    }

    // 7. Platform Services
    auto find_pid = [](const std::string& comm_name) -> pid_t {
        DIR* dir = opendir("/proc");
        if (!dir) return 0;
        struct dirent* ent;
        pid_t found_pid = 0;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_type == DT_DIR) {
                std::string pid_str = ent->d_name;
                if (!pid_str.empty() && std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) {
                    std::ifstream cf("/proc/" + pid_str + "/comm");
                    std::string comm;
                    if (cf >> comm && comm == comm_name) {
                        found_pid = static_cast<pid_t>(std::stoi(pid_str));
                        break;
                    }
                }
            }
        }
        closedir(dir);
        return found_pid;
    };

    struct DaemonMeta { const char* bin; const char* role; };
    DaemonMeta metas[] = {
        {"tinexus-serviced", "Platform Supervisor (Supervision Tree)"},
        {"tinexus-comp",     "Wayland Vulkan Compositor"},
        {"tinexus-ipcd",     "IPC Broker & Router Daemon"},
        {"tinexus-searchd",  "Ranking Engine & Index Daemon"},
        {"tinexus-notif",    "Desktop Notification Daemon"}
    };

    m_services.clear();
    for (const auto& dm : metas) {
        pid_t pid = find_pid(dm.bin);
        ServiceSpec ss;
        ss.name = dm.bin;
        ss.role = dm.role;
        if (pid > 0) {
            ss.active = true;
            ss.status = "ACTIVE";
            ss.pid_str = "PID " + std::to_string(pid);
        } else {
            ss.active = true;
            ss.status = "STANDBY";
            ss.pid_str = "Supervised";
        }
        m_services.push_back(ss);
    }
}

void AboutWidget::switch_tab(AboutTab tab) {
    if (m_active_tab != tab) {
        m_active_tab = tab;
        if (m_tabbar && m_tabbar->selected_index() != static_cast<size_t>(tab)) {
            m_tabbar->set_selected_index(static_cast<size_t>(tab));
        }
        mark_needs_paint();
    }
}

txui::Size AboutWidget::measure_override(const txui::Constraints& c) noexcept {
    return txui::Size(c.max_width, c.max_height);
}

void AboutWidget::layout_override(const txui::Rect& frame) noexcept {
    // 1. Top Segmented Tabs Bar (Centered at top of window frame)
    const double tabbar_w = 440.0;
    const double tabbar_h = 32.0;
    const double tabbar_x = frame.x() + (frame.width() - tabbar_w) * 0.5;
    const double tabbar_y = frame.y() + 12.0;
    if (m_tabbar) {
        m_tabbar->layout(txui::Rect(tabbar_x, tabbar_y, tabbar_w, tabbar_h));
    }

    // 2. Content area layout with strict 24px left/right/bottom margins
    const double pad_left = 24.0;
    const double pad_top = 56.0;
    const double pad_right = 24.0;
    const double pad_bottom = 24.0;
    m_content_rect = txui::Rect(frame.x() + pad_left,
                                frame.y() + pad_top,
                                frame.width() - pad_left - pad_right,
                                frame.height() - pad_top - pad_bottom);

    // 3. Overview action buttons layout (Positioned inside the right pane, below the specs card)
    const double sep_x = m_content_rect.x() + 195.0;
    const double rx = sep_x + 20.0;
    const double card_y = m_content_rect.y() + 54.0;
    const double card_h = 186.0;
    const double by = card_y + card_h + 12.0;
    const double btn1_w = 145.0;
    const double btn2_w = 155.0;
    const double btn_gap = 12.0;
    const double btn_x1 = rx + 410.0 - (btn1_w + btn_gap + btn2_w);
    const double btn_x2 = btn_x1 + btn1_w + btn_gap;

    if (m_btn_report) {
        m_btn_report->layout(txui::Rect(btn_x1, by, btn1_w, 32.0));
    }
    if (m_btn_update) {
        m_btn_update->layout(txui::Rect(btn_x2, by, btn2_w, 32.0));
    }
}

namespace {
// Helper for truncating long spec values with FontMetrics-verified ellipsis
std::string fit_spec_text(std::string_view text, double max_width, double font_size, bool bold = false) {
    auto ext = txui::FontMetrics::measure(text, font_size, bold);
    if (ext.width <= max_width) {
        return std::string(text);
    }
    std::string truncated(text);
    while (!truncated.empty()) {
        truncated.pop_back();
        auto t_ext = txui::FontMetrics::measure(truncated + "...", font_size, bold);
        if (t_ext.width <= max_width) {
            return truncated + "...";
        }
    }
    return "...";
}
} // namespace

void AboutWidget::paint_override(txui::Painter& painter) const noexcept {
    using namespace colors;
    const auto& f = frame();

    // ── 1. Window Background ────────────────────────────────────────────────
    painter.fill_rect(f, BG_WINDOW);

    // ── 2. Top Segmented Tabs Bar (Rendered via txui::SegmentedControl) ─────
    if (m_tabbar) {
        m_tabbar->paint(painter);
    }

    // ── 3. Left Emblem Section (Optically Centered via FontMetrics) ─────────
    const double emblem_cx = m_content_rect.x() + 92.0;
    const double emblem_cy = m_content_rect.y() + 78.0;

    // Ambient drop glow around logo
    painter.draw_glow({emblem_cx, emblem_cy}, 42.0, 88.0, txui::Color(124, 58, 237, 75));
    painter.draw_glow({emblem_cx, emblem_cy}, 18.0, 52.0, txui::Color(56, 189, 248, 65));

    // Official Tinexus Nexus Star Logo
    auto logo_buf = logo::get_logo(128);
    if (logo_buf.is_valid()) {
        constexpr double logo_size = 104.0;
        painter.draw_image({emblem_cx - logo_size * 0.5, emblem_cy - logo_size * 0.5, logo_size, logo_size},
                           logo_buf.pixels, logo_buf.width, logo_buf.height);
    } else {
        painter.fill_circle({emblem_cx, emblem_cy}, 48.0, txui::Color(20, 24, 38, 255));
        painter.draw_circle({emblem_cx, emblem_cy}, 48.0, 2.0, ACCENT_CYAN);
    }

    // Brand Label below Logo — Centered via real FontMetrics
    const std::string brand_title = "Tinexus OS";
    const auto brand_ext = txui::FontMetrics::measure(brand_title, 18.0, true);
    painter.draw_text({emblem_cx - brand_ext.width * 0.5, emblem_cy + 66.0}, brand_title, TEXT_PRI, 18.0, true);

    // Platform Subtitle — Centered via real FontMetrics
    const std::string plat_sub = "Wayland Desktop Platform";
    const auto plat_ext = txui::FontMetrics::measure(plat_sub, 12.0, false);
    painter.draw_text({emblem_cx - plat_ext.width * 0.5, emblem_cy + 90.0}, plat_sub, ACCENT_CYAN, 12.0);

    // Release Tag — Centered via real FontMetrics
    const std::string rel_tag = "Version 1.0 (LTS)";
    const auto rel_ext = txui::FontMetrics::measure(rel_tag, 11.0, false);
    painter.draw_text({emblem_cx - rel_ext.width * 0.5, emblem_cy + 110.0}, rel_tag, TEXT_DIM, 11.0);

    // ── 4. Vertical Separator ───────────────────────────────────────────────
    const double sep_x = m_content_rect.x() + 195.0;
    painter.draw_line({sep_x, m_content_rect.y() + 10.0},
                      {sep_x, m_content_rect.y() + m_content_rect.height() - 10.0},
                      1.0, BORDER_SUBTLE);

    // ── 5. Right Content Pane (Strictly bounded, No Overflow) ───────────────
    const double rx = sep_x + 20.0;
    const double ry = m_content_rect.y() + 6.0;
    const double card_w = 410.0;

    if (m_active_tab == AboutTab::Overview) {
        // Title & Subtitle with proper breathing margin
        painter.draw_text({rx, ry}, m_os_title, TEXT_PRI, 18.0, true);
        painter.draw_text({rx, ry + 26.0}, m_os_version, ACCENT_CYAN, 12.0);
        painter.draw_line({rx, ry + 46.0}, {rx + card_w, ry + 46.0}, 1.0, BORDER_SUBTLE);

        // Hardware & Platform Specifications Grouped Card
        const double card_x = rx, card_y = ry + 54.0, card_h = 186.0;
        painter.fill_rounded_rect({card_x - 1.0, card_y - 1.0, card_w + 2.0, card_h + 2.0}, 11.0, txui::Color(255, 255, 255, 16));
        painter.fill_rounded_rect({card_x, card_y, card_w, card_h}, 10.0, txui::Color(30, 33, 44, 200));

        struct SpecItem { const char* label; std::string value; };
        std::vector<SpecItem> specs = {
            {"Processor",    m_cpu_model},
            {"Memory",       m_mem_info},
            {"Graphics",     m_comp_info},
            {"Platform",     "Wayland Native / Pure C++20"},
            {"Kernel",       m_kernel_version},
            {"Root Device",  m_storage_spec.device}
        };

        const double row_h = 31.0;
        const double val_avail_w = card_w - 108.0 - 14.0; // 288.0px available for value

        for (size_t i = 0; i < specs.size(); ++i) {
            double sy = card_y + static_cast<double>(i) * row_h;
            if (i > 0) {
                painter.draw_line({card_x + 14.0, sy}, {card_x + card_w - 14.0, sy}, 1.0, txui::Color(255, 255, 255, 10));
            }
            painter.draw_text({card_x + 16.0, sy + 7.5}, specs[i].label, TEXT_SEC, 12.0, true);
            
            // Render value with FontMetrics overflow prevention
            std::string fitted_val = fit_spec_text(specs[i].value, val_avail_w, 12.0, false);
            painter.draw_text({card_x + 108.0, sy + 7.5}, fitted_val, TEXT_PRI, 12.0);
        }

        // Action Buttons: Rendered via txui::Button widgets
        if (m_btn_report) m_btn_report->paint(painter);
        if (m_btn_update) m_btn_update->paint(painter);

    } else if (m_active_tab == AboutTab::Displays) {
        painter.draw_text({rx, ry}, "Displays & Graphics", TEXT_PRI, 18.0, true);
        painter.draw_text({rx, ry + 26.0}, "High DPI Hardware Accelerated Display Engine", ACCENT_CYAN, 12.0);
        painter.draw_line({rx, ry + 46.0}, {rx + card_w, ry + 46.0}, 1.0, BORDER_SUBTLE);

        // Compact sleek monitor badge & status
        const double mx = rx + 6.0, my = ry + 58.0;
        painter.fill_rounded_rect({mx - 2.0, my - 2.0, 58.0, 38.0}, 6.0, ACCENT_BLUE);
        painter.fill_rounded_rect({mx, my, 54.0, 34.0}, 5.0, txui::Color(32, 36, 48, 255));
        painter.fill_rect({mx + 4.0, my + 4.0, 46.0, 26.0}, txui::Color(14, 16, 24, 255));
        painter.fill_circle({mx + 27.0, my + 17.0}, 3.0, ACCENT_CYAN);
        // Stand
        painter.fill_rect({mx + 23.0, my + 34.0, 8.0, 6.0}, txui::Color(55, 60, 75, 255));
        painter.fill_rounded_rect({mx + 15.0, my + 40.0, 24.0, 3.0}, 1.0, txui::Color(75, 82, 98, 255));

        // Display Header info
        painter.draw_text({rx + 72.0, my + 6.0}, m_display_spec.name, TEXT_PRI, 14.0, true);
        painter.draw_text({rx + 72.0, my + 26.0}, "Primary Display • Active Hardware VSync", ACCENT_GREEN, 12.0);

        painter.draw_line({rx, ry + 112.0}, {rx + card_w, ry + 112.0}, 1.0, BORDER_SUBTLE);

        // Full-width specs table with FontMetrics ellipsis protection
        struct DispItem { const char* label; std::string val; };
        DispItem items[] = {
            {"Resolution",     m_display_spec.resolution},
            {"Refresh Rate",   m_display_spec.refresh_rate},
            {"UI Scaling",     m_display_spec.scale},
            {"Color Format",   m_display_spec.format},
            {"Render Engine",  m_display_spec.renderer}
        };

        const double disp_avail_w = card_w - 110.0 - 10.0;
        double dy = ry + 125.0;
        for (const auto& it : items) {
            painter.draw_text({rx, dy}, it.label, TEXT_DIM, 12.0, true);
            std::string fitted_val = fit_spec_text(it.val, disp_avail_w, 12.0);
            painter.draw_text({rx + 110.0, dy}, fitted_val, TEXT_PRI, 12.0);
            dy += 24.0;
        }

    } else if (m_active_tab == AboutTab::Storage) {
        painter.draw_text({rx, ry}, "Storage Management", TEXT_PRI, 18.0, true);

        std::ostringstream ss;
        ss << m_storage_spec.used_gb << " GB used of " << m_storage_spec.total_gb << " GB ("
           << static_cast<int>(m_storage_spec.used_pct * 100.0) << "% allocated)";
        painter.draw_text({rx, ry + 26.0}, ss.str(), ACCENT_CYAN, 12.0);
        painter.draw_line({rx, ry + 46.0}, {rx + card_w, ry + 46.0}, 1.0, BORDER_SUBTLE);

        // Visual Storage Bar
        const double bar_x = rx, bar_y = ry + 68.0, bar_w = 400.0, bar_h = 22.0;
        painter.fill_rounded_rect({bar_x - 1.0, bar_y - 1.0, bar_w + 2.0, bar_h + 2.0}, 7.0, BORDER_SUBTLE);
        painter.fill_rounded_rect({bar_x, bar_y, bar_w, bar_h}, 6.0, txui::Color(35, 38, 48, 255));

        double fill_w = std::clamp(bar_w * m_storage_spec.used_pct, 16.0, bar_w);
        painter.fill_rounded_rect({bar_x, bar_y, fill_w, bar_h}, 6.0, ACCENT_BLUE);

        // Sub-partition accent in used space
        double sys_w = std::clamp(fill_w * 0.65, 8.0, fill_w);
        painter.fill_rounded_rect({bar_x, bar_y, sys_w, bar_h}, 6.0, ACCENT_CYAN);

        // Legend
        double ly2 = bar_y + 34.0;
        painter.fill_circle({rx + 8.0, ly2 + 5.0}, 5.0, ACCENT_CYAN);
        painter.draw_text({rx + 20.0, ly2}, "System & Core (Rootfs)", TEXT_PRI, 12.0);

        painter.fill_circle({rx + 195.0, ly2 + 5.0}, 5.0, ACCENT_BLUE);
        painter.draw_text({rx + 207.0, ly2}, "Apps & User Space", TEXT_PRI, 12.0);

        painter.fill_circle({rx + 8.0, ly2 + 25.0}, 5.0, txui::Color(80, 85, 100, 255));
        painter.draw_text({rx + 20.0, ly2 + 20.0}, "Available Free Space", TEXT_SEC, 12.0);

        // Storage Detail table
        double dy2 = ly2 + 50.0;
        struct StorDetail { const char* label; std::string val; };
        StorDetail details[] = {
            {"Mount Point", m_storage_spec.root_mount},
            {"Device",      m_storage_spec.device},
            {"Filesystem",  m_storage_spec.fs_type},
            {"Free Space",  std::to_string(m_storage_spec.free_gb) + " GB Available"}
        };
        for (const auto& d : details) {
            painter.draw_text({rx, dy2}, d.label, TEXT_DIM, 12.0, true);
            painter.draw_text({rx + 110.0, dy2}, d.val, TEXT_PRI, 12.0);
            dy2 += 22.0;
        }

    } else if (m_active_tab == AboutTab::Support) {
        painter.draw_text({rx, ry}, "Support & Resources", TEXT_PRI, 18.0, true);
        painter.draw_text({rx, ry + 26.0}, "Engineering Documentation & Platform Guides", ACCENT_CYAN, 12.0);
        painter.draw_line({rx, ry + 46.0}, {rx + card_w, ry + 46.0}, 1.0, BORDER_SUBTLE);

        double sy = ry + 62.0;
        struct ResItem { const char* title; const char* subtitle; };
        ResItem items[] = {
            {"Platform Architecture Spec", "docs/03_SYSTEM_ARCHITECTURE.md"},
            {"Wayland Protocols & Layer Shell", "docs/14_WAYLAND_PROTOCOLS.md"},
            {"Performance Guidelines & Budgets", "docs/08_PERFORMANCE.md"},
            {"Source Code Repository", "github.com/itzabhishekgour/TinexusShell"},
            {"Open-Source Licenses", "GNU GPL v2.0 (Core) & Apache 2.0 (SDK)"}
        };

        for (const auto& it : items) {
            painter.fill_circle({rx + 6.0, sy + 6.0}, 3.5, ACCENT_BLUE);
            painter.draw_text({rx + 16.0, sy}, it.title, TEXT_PRI, 13.0, true);
            painter.draw_text({rx + 16.0, sy + 16.0}, it.subtitle, TEXT_SEC, 11.0);
            sy += 36.0;
        }

    } else if (m_active_tab == AboutTab::Service) {
        painter.draw_text({rx, ry}, "Platform Supervision", TEXT_PRI, 18.0, true);
        painter.draw_text({rx, ry + 26.0}, "Supervision Tree & Microservices Status", ACCENT_CYAN, 12.0);
        painter.draw_line({rx, ry + 46.0}, {rx + card_w, ry + 46.0}, 1.0, BORDER_SUBTLE);

        double sy = ry + 60.0;
        for (const auto& d : m_services) {
            txui::Color dot_col = (d.status == "ACTIVE") ? ACCENT_GREEN : ACCENT_CYAN;
            painter.fill_circle({rx + 6.0, sy + 6.0}, 4.5, dot_col);
            painter.draw_text({rx + 18.0, sy}, d.name, TEXT_PRI, 13.0, true);
            painter.draw_text({rx + 18.0, sy + 16.0}, d.role, TEXT_DIM, 11.0);

            painter.draw_text({rx + 245.0, sy}, d.pid_str, TEXT_SEC, 11.0);
            painter.draw_text({rx + 335.0, sy}, d.status, dot_col, 11.0, true);
            sy += 34.0;
        }
    }

    // ── 6. Bottom Notice ───────────────────────────────────────────────────
    const std::string notice = "Tinexus Platform • Built with Pure C++20 & Wayland";
    const auto notice_ext = txui::FontMetrics::measure(notice, 11.0);
    painter.draw_text({f.x() + (f.width() - notice_ext.width) * 0.5, f.y() + f.height() - 16.0},
                      notice, TEXT_DIM, 11.0);
}

bool AboutWidget::handle_event(const txui::Event& event) noexcept {
    // Dispatch to child widgets
    if (m_tabbar && m_tabbar->handle_event(event)) {
        return true;
    }
    if (m_active_tab == AboutTab::Overview) {
        if (m_btn_report && m_btn_report->handle_event(event)) {
            return true;
        }
        if (m_btn_update && m_btn_update->handle_event(event)) {
            return true;
        }
    }
    return false;
}

} // namespace tinexus::about
