#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/SegmentedControl.hpp>
#include <txui/widgets/LineGraphWidget.hpp>
#include <txui/widgets/TextInput.hpp>
#include <txui/widgets/Button.hpp>
#include <common/PlatformServices.hpp>
#include "monitor/metrics_snapshot.hpp"
#include <vector>
#include <string>
#include <chrono>

namespace tinexus::monitor {

enum class MonitorTab : size_t {
    Processes = 0,
    Performance = 1,
    Services = 2
};

enum class SortColumn {
    Name,
    PID,
    User,
    CPU,
    Memory,
    DiskIO,
    State
};

enum class ProcessCategory : size_t {
    All = 0,
    MyProcesses = 1,
    System = 2
};

class MonitorWidget : public txui::Widget {
private:
    // Top-level tab navigation
    txui::Ref<txui::SegmentedControl> m_tabbar;
    MonitorTab m_active_tab{MonitorTab::Processes};

    // Performance Tab Charts
    txui::Ref<txui::LineGraphWidget> m_cpu_graph;
    txui::Ref<txui::LineGraphWidget> m_memory_graph;
    txui::Ref<txui::LineGraphWidget> m_disk_graph;
    txui::Ref<txui::LineGraphWidget> m_network_graph;

    // Processes Tab Controls & State
    txui::Ref<txui::TextInput> m_search_input;
    txui::Ref<txui::SegmentedControl> m_category_control;
    std::string m_search_query;
    ProcessCategory m_active_category{ProcessCategory::All};
    SortColumn m_sort_column{SortColumn::CPU};
    bool m_sort_descending{true};
    int32_t m_selected_pid{-1};
    size_t m_scroll_offset{0};

    // Confirmation Modal State (Safety-Critical)
    bool m_show_kill_modal{false};
    bool m_kill_force{false};
    int32_t m_target_kill_pid{-1};
    std::string m_target_kill_name;
    bool m_target_is_protected{false};
    bool m_target_other_user{false};

    // Signal verification tracking
    int32_t m_pending_kill_pid{-1};
    std::chrono::steady_clock::time_point m_pending_kill_time{};
    uid_t m_current_uid{0};

    // Latest snapshot & services cache
    SystemSnapshot m_latest_snapshot;
    std::vector<tinexus::platform::ServiceInfo> m_services;
    std::string m_action_feedback;

    void paint_performance_tab(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_services_tab(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_processes_tab(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_kill_confirmation_modal(txui::Painter& painter) const noexcept;

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

public:
    MonitorWidget() noexcept;
    ~MonitorWidget() override = default;

    void refresh_telemetry() noexcept;
    void switch_tab(MonitorTab tab) noexcept;

    void trigger_end_process(bool force);

    // Filter & sort helper
    [[nodiscard]] std::vector<ProcessInfo> get_filtered_and_sorted_processes() const;

    // Helpers for programmatic control and testing
    void set_search_query(std::string query) noexcept;
    void set_selected_pid(int32_t pid) noexcept;
    void set_category(ProcessCategory cat) noexcept;
    void set_sort(SortColumn col, bool desc) noexcept;

    [[nodiscard]] const SystemSnapshot& snapshot() const noexcept { return m_latest_snapshot; }
    [[nodiscard]] bool is_modal_open() const noexcept { return m_show_kill_modal; }

    bool handle_event(const txui::Event& event) noexcept override;
};

} // namespace tinexus::monitor
