#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <common/logger.hpp>
#include <common/NetUtils.hpp>
#include <indexer/desktop_entry.hpp>

#include <vector>
#include <string>
#include <functional>
#include <cstdint>

namespace tinexus::shell {

enum class ResultKind { App, System, Calculator };

struct AppItem {
    std::string name;
    std::string exec;
    std::string description;
    bool is_terminal{false};
    std::string icon;
    ResultKind  kind{ResultKind::App};
};

struct ShellNotification {
    uint32_t id{0};
    std::string app_name;
    std::string title;
    std::string body;
    std::string time_ago;
    txui::Color icon_col;
    int urgency{1}; // 0: low, 1: normal, 2: critical
};

std::vector<AppItem> load_system_apps();
std::vector<AppItem> build_results(const std::string& q, const std::vector<AppItem>& all);
pid_t spawn_app(const AppItem& item);
double read_battery_percent();
void read_network_status(int& out_bars, bool& out_connected);

class DesktopShellWidget : public txui::Widget {
public:
    std::function<void(double new_h)> on_resize_requested;
    std::function<void(const AppItem& item)> on_app_launch;

    // Interactive State Flags
    bool logo_menu_open{false};
    int  logo_menu_hover{-1};

    bool app_menu_open{false};
    int  app_menu_hover{-1};

    bool calendar_open{false};
    int  calendar_nav_offset{0};
    int  calendar_day_hover{-1};

    bool notifications_open{false};
    int  notif_hover_idx{-1};
    bool notif_clear_hover{false};
    std::vector<ShellNotification> notifications;

    bool pulse_active{false};
    std::string pulse_query;
    size_t pulse_selected_index{0};
    int pulse_hovered_index{-1};
    std::vector<AppItem> pulse_results;
    std::vector<AppItem> all_apps;
    bool pulse_launching{false};
    AppItem pulse_launch_app;

    // Top Bar Hover Targets
    bool hover_logo{false};
    bool hover_app_title{false};
    bool hover_aura_date{false};
    bool hover_sun{false};
    bool hover_vol{false};
    bool hover_bat{false};
    bool hover_wifi{false};
    bool hover_bell{false};

    // System Dynamic Values
    std::string active_app_name{"Applications"};
    bool sound_muted{false};
    bool dark_theme{true};

    DesktopShellWidget();
    ~DesktopShellWidget() override = default;

    void close_all_flyouts();
    void sync_notifications();

    txui::Size measure_override(const txui::Constraints& c) noexcept override;
    void       paint_override(txui::Painter& painter)      const noexcept override;
    bool       handle_event(const txui::Event& event)       noexcept override;
};

} // namespace tinexus::shell
