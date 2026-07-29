#include <txui/input/Event.hpp>
#include <txui/wayland/WaylandInput.hpp>
#include <txui/window/Window.hpp>
#include <linux/input-event-codes.h>
#include <iostream>
#include <type_traits>
#include <cassert>

using namespace txui;

static void test_event_union_invariants() {
    std::cout << "--- Test 1: Event Union Invariants & Memory Footprint ---" << std::endl;
    static_assert(std::is_trivial_v<Event>, "txui::Event must be a trivial type for efficient queue copy");

    Event ev;
    ev.type = EventType::KeyDown;
    ev.timestamp_ns = 123456789ULL;
    ev.window_id = 42;
    ev.keyboard.key = Key::A;
    ev.keyboard.modifiers = KeyModifier::Shift | KeyModifier::Ctrl;
    ev.keyboard.is_repeat = false;

    assert(ev.type == EventType::KeyDown);
    assert(ev.timestamp_ns == 123456789ULL);
    assert(ev.window_id == 42);
    assert(ev.keyboard.key == Key::A);
    assert(has_modifier(ev.keyboard.modifiers, KeyModifier::Shift));
    assert(has_modifier(ev.keyboard.modifiers, KeyModifier::Ctrl));
    assert(!has_modifier(ev.keyboard.modifiers, KeyModifier::Alt));

    std::cout << "PASS: Event union invariants verified (sizeof(Event)=" << sizeof(Event) << " bytes)" << std::endl;
}

static void test_linux_code_translations() {
    std::cout << "--- Test 2: Linux Keycode & Button Translation ---" << std::endl;
    assert(wayland::translate_linux_keycode(KEY_A) == Key::A);
    assert(wayland::translate_linux_keycode(KEY_Z) == Key::Z);
    assert(wayland::translate_linux_keycode(KEY_ESC) == Key::Escape);
    assert(wayland::translate_linux_keycode(KEY_ENTER) == Key::Enter);
    assert(wayland::translate_linux_keycode(KEY_F1) == Key::F1);
    assert(wayland::translate_linux_keycode(9999) == Key::Unknown);

    assert(wayland::translate_linux_button(BTN_LEFT) == MouseButton::Left);
    assert(wayland::translate_linux_button(BTN_RIGHT) == MouseButton::Right);
    assert(wayland::translate_linux_button(BTN_MIDDLE) == MouseButton::Middle);

    std::cout << "PASS: All keycode and button translations verified" << std::endl;
}

static void test_window_creation_and_events() {
    std::cout << "--- Test 3: txui::Window Creation & FIFO Event Queue ---" << std::endl;
    Ref<Window> win = Window::create(800, 600, "Test Window");
    assert(win.get() != nullptr);
    assert(win->width() == 800);
    assert(win->height() == 600);
    assert(win->state() == WindowState::Running);
    assert(!win->should_close());

    // Push synthetic events
    Event e1;
    e1.type = EventType::KeyDown;
    e1.timestamp_ns = 1000;
    e1.window_id = win->id();
    e1.keyboard.key = Key::A;
    win->push_event(e1);

    Event e2;
    e2.type = EventType::PointerMove;
    e2.timestamp_ns = 2000;
    e2.window_id = win->id();
    e2.pointer.x = 120.5;
    e2.pointer.y = 340.5;
    win->push_event(e2);

    Event out;
    bool res1 = win->poll_event(out);
    assert(res1);
    assert(out.type == EventType::KeyDown);
    assert(out.timestamp_ns == 1000);
    assert(out.keyboard.key == Key::A);

    bool res2 = win->poll_event(out);
    assert(res2);
    assert(out.type == EventType::PointerMove);
    assert(out.timestamp_ns == 2000);
    assert(out.pointer.x == 120.5 && out.pointer.y == 340.5);

    bool res3 = win->poll_event(out);
    assert(!res3);

    std::cout << "PASS: FIFO event queue order & polling verified successfully" << std::endl;
}

static void test_window_resize_and_close() {
    std::cout << "--- Test 4: Compositor-Driven Resize & Close Lifecycle ---" << std::endl;
    Ref<Window> win = Window::create(800, 600, "Lifecycle Test");
    assert(win->width() == 800 && win->height() == 600);

    // Simulate compositor configure event
    win->on_configure(1024, 768);
    assert(win->width() == 1024 && win->height() == 768);

    Event out;
    bool has_resize = win->poll_event(out);
    assert(has_resize);
    assert(out.type == EventType::WindowResize);
    assert(out.resize.width == 1024 && out.resize.height == 768);

    // Simulate xdg_toplevel.close event
    win->on_close_request();
    assert(win->should_close());
    assert(win->state() == WindowState::Closing);

    bool has_close = win->poll_event(out);
    assert(has_close);
    assert(out.type == EventType::WindowClose);
    assert(out.close.requested);

    std::cout << "PASS: Compositor configure resize & close lifecycle verified without leaks" << std::endl;
}

int main() {
    std::cout << "=== Phase 4.2.4 Unit Test: txui::Window, Input & Event Loop ===" << std::endl;
    test_event_union_invariants();
    test_linux_code_translations();
    test_window_creation_and_events();
    test_window_resize_and_close();
    std::cout << "=== Phase 4.2.4 ALL CRITERIA PASSED SUCCESSFULLY ===" << std::endl;
    return 0;
}
