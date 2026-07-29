#include <txui/window/Window.hpp>
#include <txui/input/Event.hpp>
#include <txui/render/Painter.hpp>
#include <iostream>
#include <cmath>

using namespace txui;

int main() {
    std::cout << "=== Phase 4.2.4 Demo: txui::Window, Input & Wayland Event Loop ===" << std::endl;

    Ref<Window> win = Window::create(800, 600, "Tinexus Platform - Phase 4.2.4 Window & Input Demo");
    if (!win) {
        std::cerr << "ERROR: Failed to create txui::Window" << std::endl;
        return 1;
    }

    std::cout << "Window created successfully (ID=" << win->id() << ", Size="
              << win->width() << "x" << win->height() << ")" << std::endl;

    if (!win->is_wayland_connected()) {
        std::cout << "NOTE: No live Wayland compositor detected. Demonstrating event queue & lifecycle..." << std::endl;
        // Push synthetic events for demo verification
        Event e1;
        e1.type = EventType::KeyDown;
        e1.keyboard.key = Key::A;
        e1.keyboard.modifiers = KeyModifier::Shift;
        win->push_event(e1);

        Event e2;
        e2.type = EventType::PointerMove;
        e2.pointer.x = 250.0;
        e2.pointer.y = 180.0;
        win->push_event(e2);

        Event e3;
        e3.type = EventType::PointerButtonPress;
        e3.pointer.button = MouseButton::Left;
        win->push_event(e3);

        win->on_configure(1024, 768);
        win->on_close_request();
    } else {
        std::cout << "SUCCESS: Connected to live Wayland compositor! Close window or press ESC to exit." << std::endl;
    }

    uint32 frame_count = 0;
    while (!win->should_close()) {
        Event ev;
        while (win->poll_event(ev)) {
            switch (ev.type) {
                case EventType::KeyDown:
                    std::cout << "KeyDown Key=" << static_cast<int>(ev.keyboard.key)
                              << " Mods=" << static_cast<int>(ev.keyboard.modifiers) << std::endl;
                    if (ev.keyboard.key == Key::Escape) {
                        std::cout << "ESC pressed. Closing window..." << std::endl;
                        win->close();
                    }
                    break;
                case EventType::KeyUp:
                    std::cout << "KeyUp Key=" << static_cast<int>(ev.keyboard.key) << std::endl;
                    break;
                case EventType::PointerMove:
                    std::cout << "PointerMove (" << ev.pointer.x << ", " << ev.pointer.y << ")" << std::endl;
                    break;
                case EventType::PointerButtonPress:
                    std::cout << "PointerButtonPress Button=" << static_cast<int>(ev.pointer.button)
                              << " (" << (ev.pointer.button == MouseButton::Left ? "Left" : "Other") << " Button Down)" << std::endl;
                    break;
                case EventType::PointerButtonRelease:
                    std::cout << "PointerButtonRelease Button=" << static_cast<int>(ev.pointer.button)
                              << " (Left Button Up)" << std::endl;
                    break;
                case EventType::WindowResize:
                    std::cout << "WindowResize to (" << ev.resize.width << "x" << ev.resize.height
                              << ") -> double buffers reallocated without leak!" << std::endl;
                    break;
                case EventType::WindowClose:
                    std::cout << "WindowClose requested by Wayland compositor" << std::endl;
                    break;
                default:
                    break;
            }
        }

        if (win->should_close()) {
            break;
        }

        // Production Painter rendering: fill background & draw animated rounded rectangle
        win->painter().begin_frame();
        win->painter().fill_rect(Rect(0, 0, static_cast<float64>(win->width()), static_cast<float64>(win->height())),
                                 Color(25, 30, 38, 255));

        float64 padding = 40.0;
        float64 w = static_cast<float64>(win->width()) - 2.0 * padding;
        float64 h = static_cast<float64>(win->height()) - 2.0 * padding;
        if (w > 0 && h > 0) {
            win->painter().fill_rounded_rect(Rect(padding, padding, w, h),
                                             32.0,
                                             Color(51, 127, 229, 216));
        }
        win->painter().end_frame();
        win->present();

        frame_count++;
        if (!win->is_wayland_connected()) {
            // In offline simulation mode, break after processing demo events
            break;
        } else {
            win->wait();
        }
    }

    std::cout << "=== txui-demo-04-window Exited Cleanly (Frames Rendered: " << frame_count << ") ===" << std::endl;
    return 0;
}
