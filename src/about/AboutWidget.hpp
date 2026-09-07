#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/SegmentedControl.hpp>
#include <txui/widgets/Button.hpp>
#include <txui/core/Types.hpp>
#include <txui/math/Rect.hpp>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace tinexus::about {

enum class AboutTab {
    Overview,
    Displays,
    Storage,
    Support,
    Service
};

struct DisplaySpec {
    std::string name;
    std::string resolution;
    std::string refresh_rate;
    std::string scale;
    std::string format;
    std::string renderer;
};

struct StorageSpec {
    std::string root_mount;
    std::string device;
    std::string fs_type;
    uint64_t total_gb{0};
    uint64_t used_gb{0};
    uint64_t free_gb{0};
    double used_pct{0.25};
};

struct ServiceSpec {
    std::string name;
    std::string role;
    std::string status;
    std::string pid_str;
    bool active{false};
};

class AboutWidget : public txui::Widget {
public:
    AboutWidget();
    ~AboutWidget() override = default;

    bool handle_event(const txui::Event& event) noexcept override;

    void switch_tab(AboutTab tab);

protected:
    txui::Size measure_override(const txui::Constraints& c) noexcept override;
    void       layout_override(const txui::Rect& frame)    noexcept override;
    void       paint_override(txui::Painter& painter)      const noexcept override;

private:
    void read_system_info();

    AboutTab m_active_tab{AboutTab::Overview};

    // Core TxUI child widgets
    txui::Ref<txui::SegmentedControl> m_tabbar;
    txui::Ref<txui::Button>           m_btn_report;
    txui::Ref<txui::Button>           m_btn_update;

    // System info strings
    std::string m_os_title{"Tinexus Desktop"};
    std::string m_os_version{"Version 1.0 (Architecture Freeze - LTS)"};
    std::string m_cpu_model;
    std::string m_mem_info;
    std::string m_comp_info;
    std::string m_kernel_version;
    
    // Tab detailed specs
    DisplaySpec              m_display_spec;
    StorageSpec              m_storage_spec;
    std::vector<ServiceSpec> m_services;

    // UI layout bounding rect
    txui::Rect m_content_rect;
};

} // namespace tinexus::about
