#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/TextInput.hpp>
#include <txui/widgets/Icon.hpp>
#include <txui/core/Ref.hpp>
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

// NOTE: Flatpak/XWayland are NOT implemented — do not add these claims until infrastructure is built and verified.

namespace tinexus::store {

enum class InstallState {
    NotInstalled,
    Installing,
    Installed
};

struct StoreAppItem {
    std::string id;            // e.g. "org.ksnip.Ksnip"
    std::string name;          // e.g. "Ksnip"
    std::string summary;       // Short tagline
    std::string description;   // Detailed description
    std::string category;      // "Utilities", "Browsers", "Development", "Media", etc.
    std::string version;       // "1.10.1"
    std::string download_size; // "24.3 MB"
    std::string source;        // "AppImage"
    std::string exec_cmd;      // Command to launch
    std::string download_url;  // Live HTTPS download URL
    std::string sha256_hash;   // Expected SHA-256 digest
    uint64_t bytes_received{0};// Real bytes received from network
    uint64_t bytes_total{0};   // Total expected bytes
    double speed_mbps{0.0};    // Real download throughput (MB/s)
    txui::IconType icon_type{txui::IconType::Package};
    InstallState state{InstallState::NotInstalled};
    double progress{0.0};      // 0.0 to 1.0 during installation
    std::string status_text;   // e.g. "8.4 MB / 24.3 MB (2.1 MB/s)"
    bool featured{false};
};

class StoreWidget : public txui::Widget {
public:
    StoreWidget();
    ~StoreWidget() override;

    bool handle_event(const txui::Event& event) noexcept override;

    void set_category(std::string_view cat) noexcept;
    [[nodiscard]] const std::string& current_category() const noexcept { return m_current_category; }

    void set_search_query(std::string_view query) noexcept;
    [[nodiscard]] const std::string& search_query() const noexcept { return m_search_query; }

    void trigger_install(const std::string& app_id);
    void trigger_open(const std::string& app_id);
    void trigger_uninstall(const std::string& app_id);

    [[nodiscard]] const std::vector<StoreAppItem>& apps() const noexcept { return m_catalog; }
    [[nodiscard]] std::vector<const StoreAppItem*> filtered_apps() const noexcept;

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

private:
    std::vector<StoreAppItem> m_catalog;
    std::vector<std::string>  m_categories;
    std::string               m_current_category{"Discover"};
    std::string               m_search_query;
    txui::Ref<txui::TextInput> m_search_input;

    // Layout caching
    txui::Rect m_sidebar_rect;
    txui::Rect m_header_rect;
    txui::Rect m_content_rect;
    std::vector<txui::Rect> m_category_rects;
    std::vector<txui::Rect> m_card_rects;
    std::vector<txui::Rect> m_btn_rects;
    std::vector<std::string> m_visible_app_ids;

    // Interaction states
    int m_hovered_category{-1};
    int m_hovered_btn{-1};
    int m_pressed_btn{-1};
    double m_scroll_offset{0.0};
    double m_max_scroll{0.0};

    // Async worker
    std::mutex m_state_mutex;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running{true};

    void init_catalog();
    void sync_installed_status();
    void run_install_worker(std::string app_id);
};

} // namespace tinexus::store
