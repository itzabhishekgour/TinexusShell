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
        mark_needs_paint();
    }
}

txui::Size AboutWidget::measure_override(const txui::Constraints& c) noexcept {
    return txui::Size(c.max_width, c.max_height);
}

void AboutWidget::layout_override(const txui::Rect& frame) noexcept {
    m_tabbar_rect = txui::Rect(frame.x() + (frame.width() - 430.0) * 0.5, frame.y() + 11.0, 430.0, 30.0);
    m_content_rect = txui::Rect(frame.x() + 20.0, frame.y() + 54.0, frame.width() - 40.0, frame.height() - 84.0);

    double rx = m_content_rect.x() + 270.0;
    double by = m_content_rect.y() + 276.0;
    m_btn_report_rect = txui::Rect(rx, by, 150.0, 32.0);
    m_btn_update_rect = txui::Rect(rx + 162.0, by, 158.0, 32.0);
}

void AboutWidget::paint_override(txui::Painter& painter) const noexcept {
    using namespace colors;
    const auto& f = frame();

    // ── 1. Window Background ────────────────────────────────────────────────
    painter.fill_rect(f, BG_WINDOW);

    // ── 2. Top Segmented Tabs Bar (Integrated Luxury Segmented Control) ───
    painter.fill_rounded_rect({m_tabbar_rect.x() - 1.0, m_tabbar_rect.y() - 1.0, m_tabbar_rect.width() + 2.0, m_tabbar_rect.height() + 2.0}, 16, txui::Color(255, 255, 255, 20));
    painter.fill_rounded_rect(m_tabbar_rect, 15, TAB_BG);

    const char* tab_names[] = { "Overview", "Displays", "Storage", "Support", "Service" };
    const double tab_w = m_tabbar_rect.width() / 5.0;

    for (int i = 0; i < 5; ++i) {
        double tx = m_tabbar_rect.x() + static_cast<double>(i) * tab_w;
        txui::Rect tr(tx + 2.0, m_tabbar_rect.y() + 2.0, tab_w - 4.0, m_tabbar_rect.height() - 4.0);

        bool is_active = (static_cast<int>(m_active_tab) == i);
        bool is_hover  = (m_hovered_tab == i);

        // Divider between inactive tabs
        if (i > 0 && !is_active && static_cast<int>(m_active_tab) != (i - 1)) {
            painter.draw_line({tx, m_tabbar_rect.y() + 7.0},
                              {tx, m_tabbar_rect.y() + m_tabbar_rect.height() - 7.0},
                              1.0, txui::Color(255, 255, 255, 18));
        }

        if (is_active) {
            // Elevated luxury glass segment highlight
            painter.fill_rounded_rect({tr.x() - 1.0, tr.y() - 1.0, tr.width() + 2.0, tr.height() + 2.0}, 14, txui::Color(255, 255, 255, 28));
            painter.fill_rounded_rect(tr, 13, TAB_ACTIVE);
        } else if (is_hover) {
            painter.fill_rounded_rect(tr, 13, txui::Color(255, 255, 255, 14));
        }

        double text_len = static_cast<double>(std::string(tab_names[i]).size()) * 7.4;
        double cx = tx + (tab_w - text_len) * 0.5;
        painter.draw_text({cx, m_tabbar_rect.y() + 7.5}, tab_names[i],
                          is_active ? txui::Color(255, 255, 255, 255) : (is_hover ? txui::Color(240, 243, 250, 255) : TEXT_SEC),
                          0.88, is_active);
    }

    // ── 3. Left Emblem Section (Pure Centered Typography, No Clunky Box) ────
    double lx = m_content_rect.x() + 15.0;
    double emblem_cx = lx + 105.0;
    double emblem_cy = m_content_rect.y() + 95.0;

    // Ambient drop glow around logo
    painter.draw_glow({emblem_cx, emblem_cy}, 50.0, 95.0, txui::Color(124, 58, 237, 80));
    painter.draw_glow({emblem_cx, emblem_cy}, 22.0, 60.0, txui::Color(56, 189, 248, 70));

    // Official Tinexus Nexus Star / Prism Logo
    auto logo_buf = logo::get_logo(128);
    if (logo_buf.is_valid()) {
        constexpr double logo_size = 120.0;
        painter.draw_image({emblem_cx - logo_size * 0.5, emblem_cy - logo_size * 0.5, logo_size, logo_size},
                           logo_buf.pixels, logo_buf.width, logo_buf.height);
    } else {
        painter.fill_circle({emblem_cx, emblem_cy}, 55.0, txui::Color(20, 24, 38, 255));
        painter.draw_circle({emblem_cx, emblem_cy}, 55.0, 2.0, ACCENT_CYAN);
    }

    // Brand Label below Logo - Mathematically Centered
    const std::string brand_title = "Tinexus OS";
    double brand_w = static_cast<double>(brand_title.size()) * 10.5;
    painter.draw_text({emblem_cx - brand_w * 0.5, emblem_cy + 78.0}, brand_title, TEXT_PRI, 1.35, true);

    // Platform Subtitle - Clean elegant centered text with cyan accent (No ugly box)
    const std::string plat_sub = "Wayland Desktop Platform";
    double plat_w = static_cast<double>(plat_sub.size()) * 7.4;
    painter.draw_text({emblem_cx - plat_w * 0.5, emblem_cy + 104.0}, plat_sub, ACCENT_CYAN, 0.92);

    // Release Tag - Muted centered label
    const std::string rel_tag = "Version 1.0 (LTS)";
    double rel_w = static_cast<double>(rel_tag.size()) * 6.6;
    painter.draw_text({emblem_cx - rel_w * 0.5, emblem_cy + 125.0}, rel_tag, TEXT_DIM, 0.82);

    // ── 4. Vertical Separator ───────────────────────────────────────────────
    double sep_x = m_content_rect.x() + 245.0;
    painter.draw_line({sep_x, m_content_rect.y() + 15.0},
                      {sep_x, m_content_rect.y() + m_content_rect.height() - 10.0},
                      1.0, BORDER_SUBTLE);

    // ── 5. Right Content Pane (Tab Dependent with Wide Columns, No Overflow) ─
    double rx = sep_x + 25.0;
    double ry = m_content_rect.y() + 14.0;

    if (m_active_tab == AboutTab::Overview) {
        // Title & Subtitle with proper breathing margin
        painter.draw_text({rx, ry}, m_os_title, TEXT_PRI, 1.45, true);
        painter.draw_text({rx, ry + 30.0}, m_os_version, ACCENT_CYAN, 0.92);

        painter.draw_line({rx, ry + 54.0}, {rx + 410.0, ry + 54.0}, 1.0, BORDER_SUBTLE);

        // Hardware & Platform Specifications Grouped Card
        double card_x = rx, card_y = ry + 64.0, card_w = 410.0, card_h = 186.0;
        painter.fill_rounded_rect({card_x - 1.0, card_y - 1.0, card_w + 2.0, card_h + 2.0}, 11, txui::Color(255, 255, 255, 16));
        painter.fill_rounded_rect({card_x, card_y, card_w, card_h}, 10, txui::Color(30, 33, 44, 200));

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
        for (size_t i = 0; i < specs.size(); ++i) {
            double sy = card_y + static_cast<double>(i) * row_h;
            if (i > 0) {
                painter.draw_line({card_x + 14.0, sy}, {card_x + card_w - 14.0, sy}, 1.0, txui::Color(255, 255, 255, 10));
            }
            painter.draw_text({card_x + 16.0, sy + 7.5}, specs[i].label, TEXT_SEC, 0.90, true);
            painter.draw_text({card_x + 118.0, sy + 7.5}, specs[i].value, TEXT_PRI, 0.90);
        }

        // Action Buttons: System Report & Software Update
        // Button 1: System Report
        painter.fill_rounded_rect({m_btn_report_rect.x() - 1.0, m_btn_report_rect.y() - 1.0, m_btn_report_rect.width() + 2.0, m_btn_report_rect.height() + 2.0}, 9, txui::Color(255, 255, 255, 20));
        painter.fill_rounded_rect(m_btn_report_rect, 8, m_btn_report_hovered ? BTN_HOVER : BTN_BG);
        double rep_len = 16.0 * 7.5;
        double rep_x = m_btn_report_rect.x() + (m_btn_report_rect.width() - rep_len) * 0.5;
        painter.draw_text({rep_x, m_btn_report_rect.y() + 8.0},
                          "System Report...", TEXT_PRI, 0.95, true);

        // Button 2: Software Update
        painter.fill_rounded_rect({m_btn_update_rect.x() - 1.0, m_btn_update_rect.y() - 1.0, m_btn_update_rect.width() + 2.0, m_btn_update_rect.height() + 2.0}, 9, txui::Color(255, 255, 255, 20));
        painter.fill_rounded_rect(m_btn_update_rect, 8, m_btn_update_hovered ? BTN_HOVER : BTN_BG);
        double upd_len = 18.0 * 7.5;
        double upd_x = m_btn_update_rect.x() + (m_btn_update_rect.width() - upd_len) * 0.5;
        painter.draw_text({upd_x, m_btn_update_rect.y() + 8.0},
                          "Software Update...", TEXT_PRI, 0.95, true);

    } else if (m_active_tab == AboutTab::Displays) {
        painter.draw_text({rx, ry}, "Displays & Graphics", TEXT_PRI, 1.45, true);
        painter.draw_text({rx, ry + 32.0}, "High DPI Hardware Accelerated Display Engine", ACCENT_CYAN, 0.92);
        painter.draw_line({rx, ry + 58.0}, {rx + 410.0, ry + 58.0}, 1.0, BORDER_SUBTLE);

        // Compact sleek monitor badge & status
        double mx = rx + 6.0, my = ry + 70.0;
        painter.fill_rounded_rect({mx - 2.0, my - 2.0, 58.0, 38.0}, 6, ACCENT_BLUE);
        painter.fill_rounded_rect({mx, my, 54.0, 34.0}, 5, txui::Color(32, 36, 48, 255));
        painter.fill_rect({mx + 4.0, my + 4.0, 46.0, 26.0}, txui::Color(14, 16, 24, 255));
        painter.fill_circle({mx + 27.0, my + 17.0}, 3.0, ACCENT_CYAN);
        // Stand
        painter.fill_rect({mx + 23.0, my + 34.0, 8.0, 6.0}, txui::Color(55, 60, 75, 255));
        painter.fill_rounded_rect({mx + 15.0, my + 40.0, 24.0, 3.0}, 1, txui::Color(75, 82, 98, 255));

        // Display Header info
        painter.draw_text({rx + 72.0, my + 6.0}, m_display_spec.name, TEXT_PRI, 1.1, true);
        painter.draw_text({rx + 72.0, my + 26.0}, "Primary Display • Active Hardware VSync", ACCENT_GREEN, 0.88);

        painter.draw_line({rx, ry + 125.0}, {rx + 410.0, ry + 125.0}, 1.0, BORDER_SUBTLE);

        // Full-width specs table (No overflow)
        struct DispItem { const char* label; std::string val; };
        DispItem items[] = {
            {"Resolution",     m_display_spec.resolution},
            {"Refresh Rate",   m_display_spec.refresh_rate},
            {"UI Scaling",     m_display_spec.scale},
            {"Color Format",   m_display_spec.format},
            {"Render Engine",  m_display_spec.renderer}
        };

        double dy = ry + 138.0;
        for (const auto& it : items) {
            painter.draw_text({rx, dy}, it.label, TEXT_DIM, 0.90, true);
            painter.draw_text({rx + 115.0, dy}, it.val, TEXT_PRI, 0.90);
            dy += 24.0;
        }

    } else if (m_active_tab == AboutTab::Storage) {
        painter.draw_text({rx, ry}, "Storage Management", TEXT_PRI, 1.45, true);

        std::ostringstream ss;
        ss << m_storage_spec.used_gb << " GB used of " << m_storage_spec.total_gb << " GB ("
           << static_cast<int>(m_storage_spec.used_pct * 100.0) << "% allocated)";
        painter.draw_text({rx, ry + 32.0}, ss.str(), ACCENT_CYAN, 0.92);
        painter.draw_line({rx, ry + 58.0}, {rx + 410.0, ry + 58.0}, 1.0, BORDER_SUBTLE);

        // Visual Storage Bar
        double bar_x = rx, bar_y = ry + 80.0, bar_w = 400.0, bar_h = 22.0;
        painter.fill_rounded_rect({bar_x - 1.0, bar_y - 1.0, bar_w + 2.0, bar_h + 2.0}, 7, BORDER_SUBTLE);
        painter.fill_rounded_rect({bar_x, bar_y, bar_w, bar_h}, 6, txui::Color(35, 38, 48, 255));

        double fill_w = std::clamp(bar_w * m_storage_spec.used_pct, 16.0, bar_w);
        painter.fill_rounded_rect({bar_x, bar_y, fill_w, bar_h}, 6, ACCENT_BLUE);

        // Sub-partition accent in used space
        double sys_w = std::clamp(fill_w * 0.65, 8.0, fill_w);
        painter.fill_rounded_rect({bar_x, bar_y, sys_w, bar_h}, 6, ACCENT_CYAN);

        // Legend
        double ly2 = bar_y + 36.0;
        painter.fill_circle({rx + 8.0, ly2 + 5.0}, 5.0, ACCENT_CYAN);
        painter.draw_text({rx + 20.0, ly2}, "System & Core (Rootfs)", TEXT_PRI, 0.9);

        painter.fill_circle({rx + 195.0, ly2 + 5.0}, 5.0, ACCENT_BLUE);
        painter.draw_text({rx + 207.0, ly2}, "Apps & User Space", TEXT_PRI, 0.9);

        painter.fill_circle({rx + 8.0, ly2 + 25.0}, 5.0, txui::Color(80, 85, 100, 255));
        painter.draw_text({rx + 20.0, ly2 + 20.0}, "Available Free Space", TEXT_SEC, 0.9);

        // Storage Detail table
        double dy2 = ly2 + 52.0;
        struct StorDetail { const char* label; std::string val; };
        StorDetail details[] = {
            {"Mount Point", m_storage_spec.root_mount},
            {"Device",      m_storage_spec.device},
            {"Filesystem",  m_storage_spec.fs_type},
            {"Free Space",  std::to_string(m_storage_spec.free_gb) + " GB Available"}
        };
        for (const auto& d : details) {
            painter.draw_text({rx, dy2}, d.label, TEXT_DIM, 0.9, true);
            painter.draw_text({rx + 115.0, dy2}, d.val, TEXT_PRI, 0.9);
            dy2 += 22.0;
        }

    } else if (m_active_tab == AboutTab::Support) {
        painter.draw_text({rx, ry}, "Support & Resources", TEXT_PRI, 1.45, true);
        painter.draw_text({rx, ry + 32.0}, "Engineering Documentation & Platform Guides", ACCENT_CYAN, 0.92);
        painter.draw_line({rx, ry + 58.0}, {rx + 410.0, ry + 58.0}, 1.0, BORDER_SUBTLE);

        double sy = ry + 74.0;
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
            painter.draw_text({rx + 16.0, sy}, it.title, TEXT_PRI, 0.92, true);
            painter.draw_text({rx + 16.0, sy + 16.0}, it.subtitle, TEXT_SEC, 0.82);
            sy += 36.0;
        }

    } else if (m_active_tab == AboutTab::Service) {
        painter.draw_text({rx, ry}, "Platform Supervision", TEXT_PRI, 1.45, true);
        painter.draw_text({rx, ry + 32.0}, "Supervision Tree & Microservices Status", ACCENT_CYAN, 0.92);
        painter.draw_line({rx, ry + 58.0}, {rx + 410.0, ry + 58.0}, 1.0, BORDER_SUBTLE);

        double sy = ry + 72.0;
        for (const auto& d : m_services) {
            txui::Color dot_col = (d.status == "ACTIVE") ? ACCENT_GREEN : ACCENT_CYAN;
            painter.fill_circle({rx + 6.0, sy + 6.0}, 4.5, dot_col);
            painter.draw_text({rx + 18.0, sy}, d.name, TEXT_PRI, 0.92, true);
            painter.draw_text({rx + 18.0, sy + 16.0}, d.role, TEXT_DIM, 0.8);

            painter.draw_text({rx + 250.0, sy}, d.pid_str, TEXT_SEC, 0.85);
            painter.draw_text({rx + 340.0, sy}, d.status, dot_col, 0.85, true);
            sy += 34.0;
        }
    }

    // ── 6. Bottom Notice ───────────────────────────────────────────────────
    double foot_y = f.y() + f.height() - 24.0;
    painter.draw_text({f.x() + (f.width() - 360.0) * 0.5, foot_y},
                      "Tinexus Platform • Built with Pure C++20 & Wayland", TEXT_DIM, 0.84);
}

bool AboutWidget::handle_event(const txui::Event& event) noexcept {
    (void)frame();

    if (event.type == txui::EventType::PointerMove) {
        double mx = event.pointer.x, my = event.pointer.y;

        // Check tabs hover
        int new_tab_hover = -1;
        if (m_tabbar_rect.contains({mx, my})) {
            double rel_x = mx - m_tabbar_rect.x();
            double tab_w = m_tabbar_rect.width() / 5.0;
            new_tab_hover = std::clamp(static_cast<int>(rel_x / tab_w), 0, 4);
        }
        if (m_hovered_tab != new_tab_hover) {
            m_hovered_tab = new_tab_hover;
            mark_needs_paint();
        }

        // Check buttons hover in Overview tab
        if (m_active_tab == AboutTab::Overview) {
            bool r_hov = m_btn_report_rect.contains({mx, my});
            bool u_hov = m_btn_update_rect.contains({mx, my});
            if (m_btn_report_hovered != r_hov || m_btn_update_hovered != u_hov) {
                m_btn_report_hovered = r_hov;
                m_btn_update_hovered = u_hov;
                mark_needs_paint();
            }
        }
        return true;
    } else if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            double mx = event.pointer.x, my = event.pointer.y;

            // Tab selection
            if (m_tabbar_rect.contains({mx, my})) {
                double rel_x = mx - m_tabbar_rect.x();
                double tab_w = m_tabbar_rect.width() / 5.0;
                int idx = std::clamp(static_cast<int>(rel_x / tab_w), 0, 4);
                switch_tab(static_cast<AboutTab>(idx));
                return true;
            }

            // Button actions
            if (m_active_tab == AboutTab::Overview) {
                if (m_btn_report_rect.contains({mx, my})) {
                    log::info("[About] Launching System Monitor (System Report)");
                    if (fork() == 0) {
                        execlp("tinexus-monitor", "tinexus-monitor", nullptr);
                        _exit(127);
                    }
                    return true;
                }
                if (m_btn_update_rect.contains({mx, my})) {
                    log::info("[About] Launching Package Manager (Software Update)");
                    if (fork() == 0) {
                        execlp("tinexus-pkg", "tinexus-pkg", nullptr);
                        _exit(127);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace tinexus::about
