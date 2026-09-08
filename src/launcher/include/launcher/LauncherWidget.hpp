#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/TextInput.hpp>
#include <txui/widgets/Icon.hpp>
#include <txui/render/FontMetrics.hpp>
#include <vector>
#include <string>
#include <string_view>
#include <functional>

namespace tinexus::launcher {

enum class ResultKind {
    App,
    System,
    Calculator,
    Store
};

struct AppItem {
    std::string name;
    std::string exec;
    std::string description;
    bool is_terminal{false};
    std::string icon;
    ResultKind kind{ResultKind::App};
};

class LauncherWidget : public txui::Widget {
private:
    txui::Ref<txui::TextInput> m_search_input;
    std::vector<AppItem> m_all_apps;
    std::vector<AppItem> m_results;
    std::vector<AppItem> m_recent_launches;
    size_t m_selected_index{0};
    std::string m_query;

    std::function<void(const AppItem&)> m_on_launch;
    std::function<void()> m_on_close;

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

public:
    LauncherWidget() noexcept;
    ~LauncherWidget() override = default;

    void set_all_apps(std::vector<AppItem> apps) noexcept;
    [[nodiscard]] const std::vector<AppItem>& all_apps() const noexcept { return m_all_apps; }

    void set_recent_launches(std::vector<AppItem> recents) noexcept;
    [[nodiscard]] const std::vector<AppItem>& recent_launches() const noexcept { return m_recent_launches; }

    void set_query(std::string_view query) noexcept;
    [[nodiscard]] const std::string& query() const noexcept { return m_query; }

    void set_selected_index(size_t index) noexcept;
    [[nodiscard]] size_t selected_index() const noexcept { return m_selected_index; }

    [[nodiscard]] const std::vector<AppItem>& results() const noexcept { return m_results; }
    [[nodiscard]] const txui::Ref<txui::TextInput>& search_input() const noexcept { return m_search_input; }

    void set_on_launch(std::function<void(const AppItem&)> callback) noexcept { m_on_launch = std::move(callback); }
    void set_on_close(std::function<void()> callback) noexcept { m_on_close = std::move(callback); }

    void refresh_results() noexcept;
    void launch_selected() noexcept;

    bool handle_event(const txui::Event& event) noexcept override;

    static bool try_eval_calc(std::string_view query, double& result) noexcept;
    static std::vector<AppItem> get_system_actions(std::string_view lq) noexcept;
    static txui::IconType resolve_icon_type(const AppItem& item) noexcept;
};

} // namespace tinexus::launcher
