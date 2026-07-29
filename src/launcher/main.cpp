#include <txui/window/Window.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <common/logger.hpp>
#include <linux/input-event-codes.h>

using namespace tinexus;

int main() {
    log::set_component_name("launcher");
    log::info("Phase A: tinexus-launcher (Milestone 3) starting...");

    auto window = txui::Window::create(700, 500, "Tinexus Launcher");

    auto root = txui::make_ref<txui::FlexLayout>();
    root->set_direction(txui::FlexDirection::Column);
    root->set_main_axis_alignment(txui::MainAxisAlignment::SpaceEvenly);
    root->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);

    // Colored blocks for Search, Grid etc.
    auto search_color = txui::make_ref<txui::SolidColorWidget>(txui::Color(255, 255, 255, 127));
    auto search_box = txui::make_ref<txui::SizedBox>(600, 50);
    search_box->add_child(search_color);
    
    auto grid_color = txui::make_ref<txui::SolidColorWidget>(txui::Color(0, 0, 0, 127));
    auto grid_box = txui::make_ref<txui::SizedBox>(600, 300);
    grid_box->add_child(grid_color);

    root->add_child(search_box);
    root->add_child(grid_box);

    window->set_root_widget(root);

    // Run event loop
    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else if (event.type == txui::EventType::KeyDown) {
                if (event.keyboard.key == txui::Key::Escape) {
                    running = false;
                }
            }
        }
        
        window->present();
        window->wait();
    }
    
    return 0;
}
