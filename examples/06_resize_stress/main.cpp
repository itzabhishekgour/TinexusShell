#include <txui/window/Window.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

using namespace txui;

int main() {
    std::cout << "=== Phase 4.3 Demo: Interactive Resize Stress ===\n";

    auto win = Window::create(800, 600, "Resize Stress Test");
    if (!win) {
        std::cerr << "Failed to connect to Wayland.\n";
        return 1;
    }

    auto root = make_ref<Row>();
    root->add_child(FlexItem::Expanded(make_ref<SolidColorWidget>(Color(255,100,100,255)), 1));
    root->add_child(FlexItem::Expanded(make_ref<SolidColorWidget>(Color(100,255,100,255)), 2));
    root->add_child(FlexItem::Expanded(make_ref<SolidColorWidget>(Color(100,100,255,255)), 1));
    
    win->set_root_widget(root);

    const std::vector<Size> sizes = {
        Size(800, 600),
        Size(1024, 768),
        Size(640, 480),
        Size(1920, 1080)
    };

    std::cout << "Starting 500 resize cycles...\n";
    
    for (int i = 0; i < 500; ++i) {
        if (win->should_close()) break;

        const Size& sz = sizes[i % sizes.size()];
        
        // Push resize locally. A real WM might not respect this, but we are stress testing
        // our own buffer reallocation and rendering pipeline.
        win->on_configure(static_cast<uint32>(sz.width), static_cast<uint32>(sz.height));
        win->present();
        
        // Pump events
        Event event;
        while (win->poll_event(event)) {
            if (event.type == EventType::KeyDown && event.keyboard.key == Key::Escape) {
                win->close();
            }
        }
        
        // Give Wayland time to flush and display
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "Interactive Resize Stress Test complete.\n";
    return 0;
}
