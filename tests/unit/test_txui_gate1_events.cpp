#include <txui/window/Window.hpp>
#include <txui/input/Event.hpp>
#include <iostream>
#include <cassert>

using namespace txui;

void test_event_queue_overflow() {
    auto win = Window::create(100, 100, "Test");
    
    // Set capacity to 1
    win->set_event_queue_capacity(1);
    
    // Push 100,000 events
    for (int i = 0; i < 100000; ++i) {
        Event e;
        e.type = EventType::PointerMove;
        e.pointer.x = static_cast<double>(i);
        win->push_event(e);
    }
    
    // Verify policy (Drop Oldest)
    Event popped;
    bool has_event = win->poll_event(popped);
    
    assert(has_event);
    assert(popped.type == EventType::PointerMove);
    assert(popped.pointer.x == 99999.0); // The last one pushed
    
    // Queue should now be empty
    assert(!win->poll_event(popped));
}

void test_event_queue_mixed_fifo() {
    auto win = Window::create(100, 100, "Test");
    win->set_event_queue_capacity(100000);
    
    // Push 100,000 mixed events
    for (int i = 0; i < 100000; ++i) {
        Event e;
        if (i % 3 == 0) {
            e.type = EventType::PointerMove;
            e.pointer.x = static_cast<double>(i);
        } else if (i % 3 == 1) {
            e.type = EventType::KeyDown;
            e.keyboard.key = Key::A;
        } else {
            e.type = EventType::WindowResize;
            e.resize.width = static_cast<uint32_t>(i);
            e.resize.height = static_cast<uint32_t>(i * 2);
        }
        win->push_event(e);
    }
    
    // Verify FIFO ordering
    for (int i = 0; i < 100000; ++i) {
        Event popped;
        bool has_event = win->poll_event(popped);
        assert(has_event);
        
        if (i % 3 == 0) {
            assert(popped.type == EventType::PointerMove);
            assert(popped.pointer.x == static_cast<double>(i));
        } else if (i % 3 == 1) {
            assert(popped.type == EventType::KeyDown);
            assert(popped.keyboard.key == Key::A);
        } else {
            assert(popped.type == EventType::WindowResize);
            assert(popped.resize.width == static_cast<uint32>(i));
        }
    }
    
    Event empty_event;
    assert(!win->poll_event(empty_event));
}

int main() {
    std::cout << "Running Gate 1: Event Queue Stress Tests...\n";
    test_event_queue_overflow();
    test_event_queue_mixed_fifo();
    std::cout << "All Event Queue Stress Tests Passed!\n";
    return 0;
}
