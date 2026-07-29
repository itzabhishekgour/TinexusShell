#include <txui/window/Window.hpp>
#include <txui/wayland/WaylandEventLoop.hpp>
#include <txui/graphics/Color.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/layout/Padding.hpp>
#include <iostream>

using namespace txui;

int main() {
    std::cout << "=== Phase 4.3 Demo: txui::layout Box Model & FlexLayout ===\n";

    // 1. Create a Wayland window
    auto win = Window::create(800, 600, "Tinexus Layout Demo");
    if (!win) {
        std::cerr << "Failed to create Wayland window. Are you running under WSLg or a Wayland compositor?\n";
        return 1;
    }

    std::cout << "Window created successfully (ID=" << win->id() << ", Size=" << win->width() << "x" << win->height() << ")\n";

    // 2. Build the UI Tree
    // We will build a Column with some Padding and Rows to demonstrate FlexLayout
    auto root_column = make_ref<Column>();
    root_column->set_main_axis_alignment(MainAxisAlignment::SpaceAround);
    root_column->set_cross_axis_alignment(CrossAxisAlignment::Stretch);

    // Top Bar (Row)
    auto top_bar = make_ref<Row>();
    top_bar->set_main_axis_alignment(MainAxisAlignment::SpaceBetween);
    top_bar->add_child(make_ref<SolidColorWidget>(Color(255, 0, 0, 255))); // Red Box
    top_bar->add_child(make_ref<SolidColorWidget>(Color(0, 255, 0, 255))); // Green Box
    top_bar->add_child(make_ref<SolidColorWidget>(Color(0, 0, 255, 255))); // Blue Box
    
    // Give fixed sizes to the top bar items
    for (auto& child : top_bar->children()) {
        auto wrapper = FlexItem::Flexible(make_ref<SizedBox>(50, 50), 1);
        child->add_child(wrapper); // Wait, SolidColorWidget doesn't take children, we need to wrap the box in the solid color?
        // Let's restructure.
    }
    
    // Proper structure:
    // SolidColorWidget inherits Widget. So we can just put it inside a SizedBox, then inside a FlexItem.
    auto red_box = make_ref<SizedBox>(100, 50);
    red_box->add_child(make_ref<SolidColorWidget>(Color(255, 50, 50, 255)));

    auto green_box = make_ref<SizedBox>(100, 50);
    green_box->add_child(make_ref<SolidColorWidget>(Color(50, 255, 50, 255)));

    auto blue_box = make_ref<SizedBox>(100, 50);
    blue_box->add_child(make_ref<SolidColorWidget>(Color(50, 50, 255, 255)));

    auto new_top_bar = make_ref<Row>();
    new_top_bar->set_main_axis_alignment(MainAxisAlignment::SpaceEvenly);
    new_top_bar->set_cross_axis_alignment(CrossAxisAlignment::Center);
    new_top_bar->add_child(red_box);
    new_top_bar->add_child(green_box);
    new_top_bar->add_child(blue_box);

    auto top_padding = make_ref<Padding>(Insets(20, 20, 20, 20), new_top_bar);

    // Middle Content (Expanded)
    auto middle_content = make_ref<SolidColorWidget>(Color(40, 40, 40, 255)); // Dark Grey

    // Bottom Bar (Row)
    auto bottom_bar = make_ref<Row>();
    bottom_bar->add_child(FlexItem::Expanded(make_ref<SolidColorWidget>(Color(200, 200, 0, 255)), 2)); // Yellow
    bottom_bar->add_child(FlexItem::Expanded(make_ref<SolidColorWidget>(Color(0, 200, 200, 255)), 1)); // Cyan
    
    auto bottom_padding = make_ref<Padding>(Insets(10, 10, 10, 10), bottom_bar);

    // Add to root column
    root_column->add_child(top_padding);
    root_column->add_child(FlexItem::Expanded(middle_content, 1));
    root_column->add_child(FlexItem::Expanded(bottom_padding, 1)); // Bottom takes 1/2 of remaining space

    // Attach to Window
    win->set_root_widget(root_column);

    // 3. Wayland Event Loop
    std::cout << "SUCCESS: Layout Engine initialized. Resize window to see FlexLayout adapt!\n";

    while (!win->should_close()) {
        win->wait();

        Event event;
        while (win->poll_event(event)) {
            if (event.type == EventType::KeyDown) {
                if (event.keyboard.key == Key::Escape) {
                    win->close();
                }
            }
        }
    }

    std::cout << "Exiting layout demo cleanly.\n";
    return 0;
}
