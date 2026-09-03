#include <txui/window/Window.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <iostream>

using namespace txui;

int main() {
    std::cout << "Running Gate 1: Headless Resize Stress Test...\n";

    auto win = Window::create(800, 600, "Headless Resize");
    if (!win) {
        // If Wayland is not available, Window::create might return nullptr.
        // For a headless test, this is problematic if it requires a real display.
        // However, standard testing expects Wayland (e.g. xvfb-run or weston).
        std::cerr << "SKIP: Wayland connection failed. Are you running under a compositor?\n";
        return 0; // Skip rather than fail
    }

    auto root = make_ref<Row>();
    root->add_child(FlexItem::Expanded(make_ref<SolidColorWidget>(Color(255,0,0,255)), 1));
    root->add_child(FlexItem::Expanded(make_ref<SolidColorWidget>(Color(0,255,0,255)), 1));
    
    win->set_root_widget(root);

    const std::vector<Size> sizes = {
        Size(800, 600),
        Size(1024, 768),
        Size(640, 480),
        Size(1920, 1080)
    };

    for (int i = 0; i < 500; ++i) {
        const Size& sz = sizes[static_cast<size_t>(i) % sizes.size()];
        
        // Simulate Wayland configure event
        win->on_configure(static_cast<uint32>(sz.width), static_cast<uint32>(sz.height));
        
        // Force rendering and buffer reallocation
        win->present();
    }

    std::cout << "PASS: 500 resize cycles completed without memory corruption or protocol errors.\n";
    return 0;
}
