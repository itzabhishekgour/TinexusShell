#include "StoreWidget.hpp"
#include <txui/render/Painter.hpp>
#include <txui/graphics/Color.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Point.hpp>
#include <txui/input/Event.hpp>
#include <common/logger.hpp>
#include <common/DBusNames.hpp>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace tinexus::store {

namespace {
    // ── Design Tokens ────────────────────────────────────────────────────────
    constexpr txui::Color BG_MAIN         { 13,  14,  18, 255};
    constexpr txui::Color BG_SIDEBAR      { 18,  19,  26, 255};
    constexpr txui::Color BORDER_SUBTLE   {255, 255, 255,  12};
    constexpr txui::Color CARD_BG         { 24,  26,  36, 255};
    constexpr txui::Color CARD_HOVER      { 32,  35,  48, 255};
    constexpr txui::Color CARD_BORDER     {255, 255, 255,  18};
    
    constexpr txui::Color TXT_TITLE       {250, 250, 255, 255};
    constexpr txui::Color TXT_BODY        {180, 185, 200, 255};
    constexpr txui::Color TXT_MUTED       {120, 125, 140, 255};
    
    constexpr txui::Color ACCENT_BLUE     { 59, 130, 246, 255}; // #3B82F6
    constexpr txui::Color ACCENT_BLUE_HOV { 96, 165, 250, 255};
    constexpr txui::Color ACCENT_GREEN    { 40, 200,  64, 255}; // #28C840
    constexpr txui::Color ACCENT_GREEN_HOV{ 65, 225,  90, 255};
    constexpr txui::Color BADGE_BG        { 45,  48,  64, 255};
    constexpr txui::Color BADGE_TXT       {160, 175, 210, 255};
    constexpr txui::Color PROGRESS_BAR_BG { 35,  38,  52, 255};

    std::string calculate_file_sha256(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";

        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
        if (!md_ctx) return "";

        EVP_DigestInit_ex(md_ctx, EVP_sha256(), nullptr);

        char buffer[65536];
        while (file.read(buffer, sizeof(buffer))) {
            EVP_DigestUpdate(md_ctx, buffer, static_cast<size_t>(file.gcount()));
        }
        if (file.gcount() > 0) {
            EVP_DigestUpdate(md_ctx, buffer, static_cast<size_t>(file.gcount()));
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len = 0;
        EVP_DigestFinal_ex(md_ctx, hash, &hash_len);
        EVP_MD_CTX_free(md_ctx);

        std::ostringstream ss;
        for (unsigned int i = 0; i < hash_len; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }
}

StoreWidget::StoreWidget() {
    m_categories = {"Discover", "Utilities", "Installed"};
    m_search_input = txui::make_ref<txui::TextInput>("Search store apps...");
    m_search_input->set_on_text_changed([this](std::string_view text) {
        set_search_query(text);
    });

    init_catalog();
    sync_installed_status();
}

StoreWidget::~StoreWidget() {
    m_running = false;
    for (auto& w : m_workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

void StoreWidget::init_catalog() {
    m_catalog = {
        {
            "org.ksnip.Ksnip",
            "Ksnip",
            "Screenshot tool with rich image annotation and editor features",
            "Ksnip is an open-source Qt-based screenshot tool that provides comprehensive image annotation features. Runs natively on Wayland compositor.",
            "Utilities",
            "1.10.1",
            "24.3 MB",
            "AppImage",
            "tx-appimage ~/.local/share/tinexus/apps/org.ksnip.Ksnip.AppImage",
            "https://github.com/ksnip/ksnip/releases/download/v1.10.1/ksnip-1.10.1-x86_64.AppImage",
            "c9c992e6f08e594c9adca68ba71ac7dd0fe2722cae5bb5ce094c431344f7ed54",
            0ULL,
            25480392ULL,
            0.0,
            txui::IconType::Image,
            InstallState::NotInstalled,
            0.0,
            "",
            true
        },
        {
            tinexus::common::dbus::app_id::Monitor,
            "Activity Monitor",
            "Native real-time telemetry, CPU, Memory, Disk, and Process Tree",
            "The core Tinexus platform Activity Monitor. Ultra-low overhead (10MB RAM), sub-millisecond CPU graph updates, and process management.",
            "Installed",
            "1.1.0",
            "12 MB",
            "System Core",
            "tinexus-monitor",
            "",
            "",
            0ULL,
            0ULL,
            0.0,
            txui::IconType::BarChart,
            InstallState::Installed,
            1.0,
            "Installed",
            false
        },
        {
            tinexus::common::dbus::app_id::Terminal,
            "Tinexus Terminal",
            "GPU-accelerated native Wayland command terminal with PTY emulation",
            "Ultra-responsive terminal built in pure C++20. Sub-1ms input latency, full ANSI color palette, and native font smoothing.",
            "Installed",
            "1.1.0",
            "8 MB",
            "System Core",
            "tinexus-terminal",
            "",
            "",
            0ULL,
            0ULL,
            0.0,
            txui::IconType::Terminal,
            InstallState::Installed,
            1.0,
            "Installed",
            false
        }
    };
}

void StoreWidget::sync_installed_status() {
    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/root";
    std::string app_dir = home_str + "/.local/share/applications/";
    std::string apps_dir = home_str + "/.local/share/tinexus/apps/";

    for (auto& item : m_catalog) {
        if (item.source == "System Core") {
            item.state = InstallState::Installed;
            item.progress = 1.0;
            continue;
        }

        // Check if desktop file or AppImage exists
        std::string dt_path = app_dir + item.id + ".desktop";
        std::string appimg_path = apps_dir + item.id + ".AppImage";
        struct stat st;
        if (stat(dt_path.c_str(), &st) == 0 || stat(appimg_path.c_str(), &st) == 0) {
            item.state = InstallState::Installed;
            item.progress = 1.0;
            item.status_text = "Installed";
            if (!item.download_url.empty()) {
                item.exec_cmd = "tx-appimage " + appimg_path;
            }
        }
    }
}

void StoreWidget::set_category(std::string_view cat) noexcept {
    m_current_category = std::string(cat);
    m_scroll_offset = 0.0;
    mark_needs_layout();
    mark_needs_paint();
}

void StoreWidget::set_search_query(std::string_view query) noexcept {
    m_search_query = std::string(query);
    m_scroll_offset = 0.0;
    mark_needs_layout();
    mark_needs_paint();
}

std::vector<const StoreAppItem*> StoreWidget::filtered_apps() const noexcept {
    std::vector<const StoreAppItem*> list;
    std::string lq = m_search_query;
    std::transform(lq.begin(), lq.end(), lq.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& app : m_catalog) {
        // Search filter takes precedence if not empty
        if (!lq.empty()) {
            std::string lname = app.name;
            std::string lsum = app.summary;
            std::string lcat = app.category;
            std::transform(lname.begin(), lname.end(), lname.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(lsum.begin(), lsum.end(), lsum.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(lcat.begin(), lcat.end(), lcat.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lname.find(lq) != std::string::npos ||
                lsum.find(lq) != std::string::npos ||
                lcat.find(lq) != std::string::npos ||
                app.id.find(lq) != std::string::npos) {
                list.push_back(&app);
            }
            continue;
        }

        // Category filter
        if (m_current_category == "Discover") {
            if (app.featured) list.push_back(&app);
        } else if (m_current_category == "Installed") {
            if (app.state == InstallState::Installed) list.push_back(&app);
        } else {
            if (app.category == m_current_category) list.push_back(&app);
        }
    }
    return list;
}

void StoreWidget::trigger_install(const std::string& app_id) {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    for (auto& item : m_catalog) {
        if (item.id == app_id) {
            if (item.state == InstallState::NotInstalled) {
                item.state = InstallState::Installing;
                item.progress = 0.05;
                item.status_text = "Downloading...";
                mark_needs_paint();

                m_workers.emplace_back(&StoreWidget::run_install_worker, this, app_id);
            }
            break;
        }
    }
}

void StoreWidget::run_install_worker(std::string app_id) {
    tinexus::log::info("[Store] Starting installation for {}", app_id);

    StoreAppItem target_item;
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        for (const auto& item : m_catalog) {
            if (item.id == app_id) {
                target_item = item;
                break;
            }
        }
    }

    if (target_item.id.empty()) {
        tinexus::log::error("[Store] App ID {} not found in catalog", app_id);
        return;
    }

    const char* home = std::getenv("HOME");
    std::string home_str = home ? home : "/root";
    std::string tinexus_dir = home_str + "/.local/share/tinexus";
    std::string apps_dir = tinexus_dir + "/apps";
    std::string app_dir = home_str + "/.local/share/applications";

    mkdir((home_str + "/.local").c_str(), 0755);
    mkdir((home_str + "/.local/share").c_str(), 0755);
    mkdir(tinexus_dir.c_str(), 0755);
    mkdir(apps_dir.c_str(), 0755);
    mkdir(app_dir.c_str(), 0755);

    if (!target_item.download_url.empty()) {
        std::string part_file = apps_dir + "/" + app_id + ".AppImage.part";
        std::string final_file = apps_dir + "/" + app_id + ".AppImage";
        unlink(part_file.c_str());

        tinexus::log::info("[Store] Downloading {} from {}...", app_id, target_item.download_url);

#ifndef _WIN32
        pid_t pid = fork();
        if (pid == 0) {
            execlp("curl", "curl", "-fSL", "-o", part_file.c_str(), target_item.download_url.c_str(), nullptr);
            _exit(127);
        }

        if (pid < 0) {
            tinexus::log::error("[Store] Failed to fork curl process");
            std::lock_guard<std::mutex> lock(m_state_mutex);
            for (auto& item : m_catalog) {
                if (item.id == app_id) {
                    item.state = InstallState::NotInstalled;
                    item.status_text = "Spawn Failed";
                    break;
                }
            }
            mark_needs_paint();
            return;
        }

        auto start_time = std::chrono::steady_clock::now();
        auto last_time = start_time;
        uint64_t last_bytes = 0;
        int status = 0;

        while (true) {
            if (!m_running) {
                kill(pid, SIGTERM);
                waitpid(pid, &status, 0);
                unlink(part_file.c_str());
                return;
            }

            pid_t res = waitpid(pid, &status, WNOHANG);
            if (res == pid) {
                break; // Process finished
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            struct stat st;
            if (stat(part_file.c_str(), &st) == 0 && st.st_size > 0) {
                uint64_t bytes_now = static_cast<uint64_t>(st.st_size);
                auto now = std::chrono::steady_clock::now();
                double dt = std::chrono::duration<double>(now - last_time).count();
                double speed_mb = 0.0;
                if (dt >= 0.25) {
                    speed_mb = static_cast<double>(bytes_now - last_bytes) / (1024.0 * 1024.0 * dt);
                    last_bytes = bytes_now;
                    last_time = now;
                }

                double mb_rec = static_cast<double>(bytes_now) / (1024.0 * 1024.0);
                double mb_tot = (target_item.bytes_total > 0) ? (static_cast<double>(target_item.bytes_total) / (1024.0 * 1024.0)) : mb_rec;
                double prog = (target_item.bytes_total > 0) ? std::clamp(static_cast<double>(bytes_now) / static_cast<double>(target_item.bytes_total), 0.01, 0.95) : 0.5;

                char stat_buf[128];
                snprintf(stat_buf, sizeof(stat_buf), "%.1f/%.1f MB (%.1f MB/s)", mb_rec, mb_tot, speed_mb);

                std::lock_guard<std::mutex> lock(m_state_mutex);
                for (auto& item : m_catalog) {
                    if (item.id == app_id) {
                        item.bytes_received = bytes_now;
                        item.speed_mbps = speed_mb;
                        item.progress = prog;
                        item.status_text = stat_buf;
                        break;
                    }
                }
                mark_needs_paint();
            }
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            tinexus::log::error("[Store] Download failed with exit code {}", WEXITSTATUS(status));
            unlink(part_file.c_str());
            std::lock_guard<std::mutex> lock(m_state_mutex);
            for (auto& item : m_catalog) {
                if (item.id == app_id) {
                    item.state = InstallState::NotInstalled;
                    item.progress = 0.0;
                    item.status_text = "Download Failed";
                    break;
                }
            }
            mark_needs_paint();
            return;
        }

        // SHA-256 Verification Step
        {
            std::lock_guard<std::mutex> lock(m_state_mutex);
            for (auto& item : m_catalog) {
                if (item.id == app_id) {
                    item.progress = 0.98;
                    item.status_text = "Verifying Checksum...";
                    break;
                }
            }
            mark_needs_paint();
        }

        std::string actual_hash = calculate_file_sha256(part_file);
        tinexus::log::info("[Store] Checksum calculated for {}: {}", app_id, actual_hash);
        tinexus::log::info("[Store] Expected checksum: {}", target_item.sha256_hash);

        std::string expected_hash = target_item.sha256_hash;
        std::string h1 = actual_hash, h2 = expected_hash;
        std::transform(h1.begin(), h1.end(), h1.begin(), ::tolower);
        std::transform(h2.begin(), h2.end(), h2.begin(), ::tolower);

        if (h1 != h2) {
            tinexus::log::error("[Store] Checksum mismatch! Verification failed.");
            unlink(part_file.c_str());
            std::lock_guard<std::mutex> lock(m_state_mutex);
            for (auto& item : m_catalog) {
                if (item.id == app_id) {
                    item.state = InstallState::NotInstalled;
                    item.progress = 0.0;
                    item.status_text = "Checksum Mismatch";
                    break;
                }
            }
            mark_needs_paint();
            return;
        }

        tinexus::log::info("[Store] Checksum verified successfully! Installing {}", app_id);

        // Atomic move & execute permissions
        if (rename(part_file.c_str(), final_file.c_str()) != 0) {
            tinexus::log::error("[Store] Failed to rename .part file to {}", final_file);
        }
        chmod(final_file.c_str(), 0755);

        // Write .desktop entry
        std::string dt_path = app_dir + "/" + app_id + ".desktop";
        std::ofstream dt(dt_path);
        if (dt.is_open()) {
            dt << "[Desktop Entry]\n";
            dt << "Type=Application\n";
            dt << "Name=" << target_item.name << "\n";
            dt << "Comment=" << target_item.summary << "\n";
            dt << "Exec=tx-appimage " << final_file << "\n";
            dt << "Icon=ksnip\n";
            dt << "Categories=" << target_item.category << ";Graphics;Utility;\n";
            dt << "StartupNotify=true\n";
            dt.close();
        }

        {
            std::lock_guard<std::mutex> lock(m_state_mutex);
            for (auto& item : m_catalog) {
                if (item.id == app_id) {
                    item.exec_cmd = "tx-appimage " + final_file;
                    item.state = InstallState::Installed;
                    item.progress = 1.0;
                    item.status_text = "Installed";
                    break;
                }
            }
        }
        tinexus::log::info("[Store] AppImage installed successfully: {}", final_file);
        mark_needs_paint();
        return;
#endif
    }

    // Fallback simulation path for items without live download URLs yet
    for (int step = 1; step <= 10; ++step) {
        if (!m_running) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        std::lock_guard<std::mutex> lock(m_state_mutex);
        for (auto& item : m_catalog) {
            if (item.id == app_id) {
                item.progress = static_cast<double>(step) / 10.0;
                int pct = static_cast<int>(item.progress * 100.0);
                item.status_text = "Installing " + std::to_string(pct) + "%";
                break;
            }
        }
        mark_needs_paint();
    }

    std::string dt_path = app_dir + "/" + app_id + ".desktop";
    std::ofstream dt(dt_path);
    if (dt.is_open()) {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        for (const auto& item : m_catalog) {
            if (item.id == app_id) {
                dt << "[Desktop Entry]\n";
                dt << "Type=Application\n";
                dt << "Name=" << item.name << "\n";
                dt << "Comment=" << item.summary << "\n";
                dt << "Exec=" << item.exec_cmd << "\n";
                dt << "Categories=" << item.category << ";\n";
                dt << "StartupNotify=true\n";
                break;
            }
        }
        dt.close();
    }

    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        for (auto& item : m_catalog) {
            if (item.id == app_id) {
                item.state = InstallState::Installed;
                item.progress = 1.0;
                item.status_text = "Installed";
                break;
            }
        }
    }
    tinexus::log::info("[Store] Installation completed successfully for {}", app_id);
    mark_needs_paint();
}

void StoreWidget::trigger_open(const std::string& app_id) {
    std::string exec;
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        for (const auto& item : m_catalog) {
            if (item.id == app_id) {
                exec = item.exec_cmd;
                break;
            }
        }
    }

    if (exec.empty()) return;
    tinexus::log::info("[Store] Launching application: {}", exec);

#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", exec.c_str(), nullptr);
        _exit(127);
    }
#endif
}

void StoreWidget::trigger_uninstall(const std::string& app_id) {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    for (auto& item : m_catalog) {
        if (item.id == app_id && item.source != "System Core") {
            item.state = InstallState::NotInstalled;
            item.progress = 0.0;
            item.status_text = "";

            const char* home = std::getenv("HOME");
            std::string app_dir = home ? (std::string(home) + "/.local/share/applications") : "/tmp/applications";
            std::string dt_path = app_dir + "/" + app_id + ".desktop";
            unlink(dt_path.c_str());

            mark_needs_paint();
            break;
        }
    }
}

bool StoreWidget::handle_event(const txui::Event& event) noexcept {
    // Forward to search input
    if (m_search_input && m_search_input->handle_event(event)) {
        mark_needs_paint();
        return true;
    }

    if (event.type == txui::EventType::PointerMove) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        int old_cat = m_hovered_category;
        m_hovered_category = -1;
        for (size_t i = 0; i < m_category_rects.size(); ++i) {
            if (m_category_rects[i].contains(txui::Point(px, py))) {
                m_hovered_category = static_cast<int>(i);
                break;
            }
        }

        int old_btn = m_hovered_btn;
        m_hovered_btn = -1;
        for (size_t i = 0; i < m_btn_rects.size(); ++i) {
            if (m_btn_rects[i].contains(txui::Point(px, py))) {
                m_hovered_btn = static_cast<int>(i);
                break;
            }
        }

        if (old_cat != m_hovered_category || old_btn != m_hovered_btn) {
            mark_needs_paint();
        }
        return true;
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            double px = event.pointer.x;
            double py = event.pointer.y;

            // Check category selection
            for (size_t i = 0; i < m_category_rects.size(); ++i) {
                if (m_category_rects[i].contains(txui::Point(px, py))) {
                    set_category(m_categories[i]);
                    return true;
                }
            }

            // Check button clicks
            for (size_t i = 0; i < m_btn_rects.size(); ++i) {
                if (m_btn_rects[i].contains(txui::Point(px, py))) {
                    m_pressed_btn = static_cast<int>(i);
                    mark_needs_paint();
                    return true;
                }
            }
        }
    }

    if (event.type == txui::EventType::PointerButtonRelease) {
        if (event.pointer.button == txui::MouseButton::Left) {
            if (m_pressed_btn >= 0 && m_pressed_btn < static_cast<int>(m_visible_app_ids.size())) {
                std::string app_id = m_visible_app_ids[static_cast<size_t>(m_pressed_btn)];
                m_pressed_btn = -1;

                InstallState cur_state = InstallState::NotInstalled;
                {
                    std::lock_guard<std::mutex> lock(m_state_mutex);
                    for (const auto& a : m_catalog) {
                        if (a.id == app_id) {
                            cur_state = a.state;
                            break;
                        }
                    }
                }

                if (cur_state == InstallState::NotInstalled) {
                    trigger_install(app_id);
                } else if (cur_state == InstallState::Installed) {
                    trigger_open(app_id);
                }
                mark_needs_paint();
                return true;
            }
            m_pressed_btn = -1;
            mark_needs_paint();
        }
    }

    if (event.type == txui::EventType::PointerScroll) {
        m_scroll_offset = std::clamp(m_scroll_offset - event.pointer.scroll_delta_y * 30.0, 0.0, m_max_scroll);
        mark_needs_paint();
        return true;
    }

    return false;
}

txui::Size StoreWidget::measure_override(const txui::Constraints& constraints) noexcept {
    return txui::Size(constraints.max_width, constraints.max_height);
}

void StoreWidget::layout_override(const txui::Rect& f) noexcept {
    const double sidebar_w = 200.0;
    const double header_h = 60.0;

    m_sidebar_rect = txui::Rect(f.x(), f.y(), sidebar_w, f.height());
    m_header_rect  = txui::Rect(f.x() + sidebar_w, f.y(), f.width() - sidebar_w, header_h);
    m_content_rect = txui::Rect(f.x() + sidebar_w, f.y() + header_h, f.width() - sidebar_w, f.height() - header_h);

    // Sidebar Category rects
    m_category_rects.clear();
    double cat_y = f.y() + 70.0;
    for (size_t i = 0; i < m_categories.size(); ++i) {
        m_category_rects.emplace_back(f.x() + 14.0, cat_y, sidebar_w - 28.0, 36.0);
        cat_y += 42.0;
    }

    // Header Search Input
    if (m_search_input) {
        double input_w = std::min(320.0, m_header_rect.width() - 40.0);
        m_search_input->layout(txui::Rect(m_header_rect.right() - input_w - 20.0, m_header_rect.y() + 14.0, input_w, 32.0));
    }

    // Cards layout
    m_card_rects.clear();
    m_btn_rects.clear();
    m_visible_app_ids.clear();

    auto visible_apps = filtered_apps();
    double card_pad = 18.0;
    double card_h   = 94.0;
    double cur_y    = m_content_rect.y() + card_pad - m_scroll_offset;
    double card_w   = m_content_rect.width() - (card_pad * 2.0);

    for (const auto* app : visible_apps) {
        txui::Rect card_r(m_content_rect.x() + card_pad, cur_y, card_w, card_h);
        m_card_rects.push_back(card_r);
        m_visible_app_ids.push_back(app->id);

        double btn_w = 110.0;
        double btn_h = 34.0;
        txui::Rect btn_r(card_r.right() - btn_w - 18.0, card_r.y() + (card_h - btn_h) / 2.0, btn_w, btn_h);
        m_btn_rects.push_back(btn_r);

        cur_y += card_h + 12.0;
    }

    double total_h = static_cast<double>(visible_apps.size()) * (card_h + 12.0) + card_pad * 2.0;
    m_max_scroll = std::max(0.0, total_h - m_content_rect.height());
}

void StoreWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto& f = frame();

    // 1. Background Fill
    painter.fill_rect(f, BG_MAIN);

    // 2. Sidebar
    painter.fill_rect(m_sidebar_rect, BG_SIDEBAR);
    painter.fill_rect(txui::Rect(m_sidebar_rect.right() - 1.0, m_sidebar_rect.y(), 1.0, m_sidebar_rect.height()), BORDER_SUBTLE);

    // Sidebar Header
    painter.draw_text(txui::Point(m_sidebar_rect.x() + 20.0, m_sidebar_rect.y() + 32.0), "Categories", TXT_MUTED, 1.0);

    // Sidebar Category Tabs
    for (size_t i = 0; i < m_categories.size(); ++i) {
        const auto& r = m_category_rects[i];
        bool is_active = (m_categories[i] == m_current_category);
        bool is_hover  = (static_cast<int>(i) == m_hovered_category);

        if (is_active) {
            painter.fill_rounded_rect(r, 8.0, ACCENT_BLUE);
            painter.draw_text(txui::Point(r.x() + 16.0, r.y() + 10.0), m_categories[i], TXT_TITLE, 1.0);
        } else {
            if (is_hover) {
                painter.fill_rounded_rect(r, 8.0, txui::Color(255, 255, 255, 12));
            }
            painter.draw_text(txui::Point(r.x() + 16.0, r.y() + 10.0), m_categories[i], is_hover ? TXT_TITLE : TXT_BODY, 1.0);
        }
    }

    // 3. Header
    painter.fill_rect(txui::Rect(m_header_rect.x(), m_header_rect.bottom() - 1.0, m_header_rect.width(), 1.0), BORDER_SUBTLE);
    painter.draw_text(txui::Point(m_header_rect.x() + 24.0, m_header_rect.y() + 22.0), m_current_category, TXT_TITLE, 1.5);

    // Render Search Input
    if (m_search_input) {
        m_search_input->paint(painter);
    }

    // 4. Content Cards
    auto visible_apps = filtered_apps();
    for (size_t i = 0; i < visible_apps.size(); ++i) {
        if (i >= m_card_rects.size()) break;
        const auto* app = visible_apps[i];
        const auto& cr  = m_card_rects[i];
        const auto& br  = m_btn_rects[i];

        // Skip rendering if completely out of content viewport
        if (cr.bottom() < m_content_rect.y() || cr.y() > m_content_rect.bottom()) {
            continue;
        }

        // Card Container
        painter.fill_rounded_rect(cr, 12.0, CARD_BG);
        painter.fill_rounded_rect(txui::Rect(cr.x() - 0.5, cr.y() - 0.5, cr.width() + 1.0, cr.height() + 1.0), 12.0, CARD_BORDER);

        // App Icon Plaque
        double icon_size = 46.0;
        txui::Rect icon_box(cr.x() + 16.0, cr.y() + (cr.height() - icon_size) / 2.0, icon_size, icon_size);
        painter.fill_rounded_rect(icon_box, 10.0, txui::Color(38, 42, 58, 255));
        txui::Icon::render(painter, app->icon_type, txui::Rect(icon_box.x() + 9.0, icon_box.y() + 9.0, 28.0, 28.0));

        // App Details
        double text_x = icon_box.right() + 16.0;
        painter.draw_text(txui::Point(text_x, cr.y() + 20.0), app->name, TXT_TITLE, 1.15);
        painter.draw_text(txui::Point(text_x, cr.y() + 42.0), app->summary, TXT_BODY, 0.95);

        // Metadata Badges (Source & Size)
        double badge_y = cr.y() + 66.0;
        std::string badge_str = app->source + " • v" + app->version + " • " + app->download_size;
        painter.draw_text(txui::Point(text_x, badge_y), badge_str, TXT_MUTED, 0.85);

        // Action Button
        bool btn_hover = (static_cast<int>(i) == m_hovered_btn);

        if (app->state == InstallState::NotInstalled) {
            txui::Color btn_col = btn_hover ? ACCENT_BLUE_HOV : ACCENT_BLUE;
            painter.fill_rounded_rect(br, 8.0, btn_col);
            painter.draw_text(txui::Point(br.x() + 32.0, br.y() + 10.0), "Get", txui::Color(255, 255, 255, 255), 1.0);
        } else if (app->state == InstallState::Installing) {
            // Progress Bar
            painter.fill_rounded_rect(br, 8.0, PROGRESS_BAR_BG);
            double fill_w = br.width() * std::clamp(app->progress, 0.05, 1.0);
            painter.fill_rounded_rect(txui::Rect(br.x(), br.y(), fill_w, br.height()), 8.0, ACCENT_BLUE);
            painter.draw_text(txui::Point(br.x() + 20.0, br.y() + 10.0), app->status_text, txui::Color(255, 255, 255, 255), 0.85);
        } else if (app->state == InstallState::Installed) {
            txui::Color btn_col = btn_hover ? ACCENT_GREEN_HOV : ACCENT_GREEN;
            painter.fill_rounded_rect(br, 8.0, btn_col);
            painter.draw_text(txui::Point(br.x() + 26.0, br.y() + 10.0), "Open", txui::Color(255, 255, 255, 255), 1.0);
        }
    }
}

} // namespace tinexus::store
