#include "layer_shell_window.hpp"
#include <txui/layout/FlexLayout.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <txui/graphics/Color.hpp>
#include <common/logger.hpp>

using namespace tinexus;

int main() {
    log::info("Phase A: tinexus-panel (Milestone 2) starting...");

    panel::LayerShellWindow window(48); // Height = 48px

    // Create the root layout (horizontal flex)
    auto root_layout = txui::make_ref<txui::FlexLayout>();
    root_layout->set_direction(txui::FlexDirection::Row);
    root_layout->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);
    root_layout->set_main_axis_alignment(txui::MainAxisAlignment::SpaceBetween);

    // Left Block (Start button placeholder)
    auto left_color = txui::make_ref<txui::SolidColorWidget>(txui::Color(51, 153, 255, 255));
    auto left_box = txui::make_ref<txui::SizedBox>(60, 32);
    left_box->add_child(left_color);
    root_layout->add_child(left_box);

    // Middle Block (Taskbar placeholder)
    auto middle_color = txui::make_ref<txui::SolidColorWidget>(txui::Color(204, 51, 153, 255));
    auto middle_box = txui::make_ref<txui::SizedBox>(300, 32);
    middle_box->add_child(middle_color);
    root_layout->add_child(middle_box);

    // Right Block (Tray placeholder)
    auto right_color = txui::make_ref<txui::SolidColorWidget>(txui::Color(51, 204, 102, 255));
    auto right_box = txui::make_ref<txui::SizedBox>(150, 32);
    right_box->add_child(right_color);
    root_layout->add_child(right_box);

    // Set root layout
    window.set_root_widget(root_layout);
    
    window.show();
    log::info("tinexus-panel layer shell successfully initialized!");

    // Enter Wayland event loop
    return window.exec();
}
