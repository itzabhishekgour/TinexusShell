#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/input/interaction_controller.hpp"
#include "comp/window/window_node.hpp"
#include "comp/window/scene_graph.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_interactive_window");
    tinexus::log::info("Running Interactive Window Drag & Resize Integration Test...");

    tinexus::comp::InteractionController controller;
    auto win = std::make_shared<tinexus::comp::WindowNode>(3003, "foot");
    win->x = 100;
    win->y = 100;
    win->width = 800;
    win->height = 600;

    // 1. Test Interactive Move
    controller.start_move(3003, 100, 100);
    assert(controller.state() == tinexus::comp::InteractionState::Moving);

    int32_t out_x = win->x, out_y = win->y, out_w = win->width, out_h = win->height;
    controller.update_drag(150, 150, out_x, out_y, out_w, out_h);
    win->x = out_x;
    win->y = out_y;

    assert(win->x == 150);
    assert(win->y == 150);
    controller.finish_operation();
    assert(controller.state() == tinexus::comp::InteractionState::Idle);

    // 2. Test Interactive Resize
    controller.start_resize(3003, tinexus::comp::ResizeEdge::BottomRight, 150, 150);
    assert(controller.state() == tinexus::comp::InteractionState::Resizing);

    controller.update_drag(200, 200, out_x, out_y, out_w, out_h);
    win->width = out_w;
    win->height = out_h;

    assert(win->width == 850);
    assert(win->height == 650);
    controller.finish_operation();
    assert(controller.state() == tinexus::comp::InteractionState::Idle);

    tinexus::log::info("Interactive Window Drag & Resize test passed 100%!");
    return 0;
}
