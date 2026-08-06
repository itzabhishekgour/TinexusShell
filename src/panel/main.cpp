#include "layer_shell_window.hpp"
#include <txui/layout/FlexLayout.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <txui/widgets/TextWidget.hpp>
#include <txui/graphics/Color.hpp>
#include <common/logger.hpp>
#include <ctime>
#include <chrono>
#include <thread>

using namespace tinexus;

static std::string get_current_time_string() {
    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char time_str[32];
    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);
    return std::string(time_str);
}

int main() {
    log::set_component_name("panel");
    log::info("[Panel] Tinexus Desktop Top Panel starting...");

    panel::LayerShellWindow window(32); // 32px height panel

    auto root_layout = txui::make_ref<txui::FlexLayout>();
    root_layout->set_direction(txui::FlexDirection::Row);
    root_layout->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);
    root_layout->set_main_axis_alignment(txui::MainAxisAlignment::SpaceBetween);

    // Left Block: Logo + Ctrl+K badge
    auto left_layout = txui::make_ref<txui::FlexLayout>();
    left_layout->set_direction(txui::FlexDirection::Row);
    left_layout->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);
    left_layout->set_main_axis_alignment(txui::MainAxisAlignment::Start);

    auto logo_bg = txui::make_ref<txui::SolidColorWidget>(txui::Color(59, 130, 246, 255)); // #3B82F6 Accent
    auto logo_text = txui::make_ref<txui::TextWidget>(" TINEXUS ", txui::Color::white(), 1.0);
    auto logo_box = txui::make_ref<txui::SizedBox>(80, 24);
    logo_box->add_child(logo_bg);
    logo_box->add_child(logo_text);

    auto shortcut_bg = txui::make_ref<txui::SolidColorWidget>(txui::Color(30, 41, 59, 220));
    auto shortcut_text = txui::make_ref<txui::TextWidget>(" [Ctrl+K Launcher] ", txui::Color(148, 163, 184, 255), 1.0);
    auto shortcut_box = txui::make_ref<txui::SizedBox>(160, 24);
    shortcut_box->add_child(shortcut_bg);
    shortcut_box->add_child(shortcut_text);

    left_layout->add_child(logo_box);
    left_layout->add_child(shortcut_box);

    auto left_container = txui::make_ref<txui::SizedBox>(260, 28);
    left_container->add_child(left_layout);

    // Middle Block: Workspace Indicator
    auto middle_bg = txui::make_ref<txui::SolidColorWidget>(txui::Color(15, 23, 42, 180));
    auto middle_text = txui::make_ref<txui::TextWidget>(" WORKSPACE: [1]  2  3 ", txui::Color(226, 232, 240, 255), 1.0);
    auto middle_box = txui::make_ref<txui::SizedBox>(220, 24);
    middle_box->add_child(middle_bg);
    middle_box->add_child(middle_text);

    // Right Block: Live Clock
    auto clock_bg = txui::make_ref<txui::SolidColorWidget>(txui::Color(15, 23, 42, 220));
    auto clock_text = txui::make_ref<txui::TextWidget>(get_current_time_string(), txui::Color::white(), 1.0);
    auto clock_box = txui::make_ref<txui::SizedBox>(100, 24);
    clock_box->add_child(clock_bg);
    clock_box->add_child(clock_text);

    root_layout->add_child(left_container);
    root_layout->add_child(middle_box);
    root_layout->add_child(clock_box);

    window.set_root_widget(root_layout);

    if (!window.is_valid()) {
        log::error("[Panel] LayerShellWindow is invalid. Exiting.");
        return 1;
    }

    window.show();
    log::info("[Panel] Top status panel initialized.");

    // Main loop: Update clock every second and process Wayland events
    while (!window.should_close()) {
        clock_text->set_text(" " + get_current_time_string() + " ");
        window.present();
        window.exec();
    }

    return 0;
}
