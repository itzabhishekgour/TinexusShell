#include "comp/window/window_state.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

using namespace tinexus::comp;

// Mock structures representing the compositor teardown mechanics
struct MockTimer {
    bool armed{false};
    int interval_ms{0};
    void* user_data{nullptr};
};

enum class MockCursorMode { Passthrough, Move, Resize };

struct MockToplevelWrapper {
    WindowStateMachine state_machine;
    MockTimer* fade_timer{nullptr};
    double opacity{1.0};
    bool is_closing{false};
    bool destroyed{false};
    int listener_link_count{8}; // Represents the 8 Wayland listener links
};

struct MockCompositorBackend {
    MockCursorMode cursor_mode{MockCursorMode::Passthrough};
    MockToplevelWrapper* grabbed_toplevel{nullptr};
    MockToplevelWrapper* active_toplevel{nullptr};
    std::vector<std::unique_ptr<MockToplevelWrapper>> toplevels;

    // The authoritative Phase-1 destruction sequence
    void destroy_toplevel(MockToplevelWrapper* wrapper) {
        if (!wrapper || wrapper->destroyed) return;

        // 1. Cancel/disarm timers
        if (wrapper->fade_timer != nullptr) {
            wrapper->fade_timer->armed = false;
            wrapper->fade_timer->user_data = nullptr;
            wrapper->fade_timer = nullptr;
        }

        // 2. Clear/release grabs
        if (grabbed_toplevel == wrapper) {
            grabbed_toplevel = nullptr;
            cursor_mode = MockCursorMode::Passthrough;
        }

        // 3. Cancel/remove references to the real wrapper
        if (active_toplevel == wrapper) {
            active_toplevel = nullptr;
        }
        wrapper->state_machine.mark_closing();

        // 4. Remove compositor registries / focus references
        wrapper->listener_link_count = 0;

        // 5. Mark destroyed and remove from container
        wrapper->destroyed = true;
        for (auto it = toplevels.begin(); it != toplevels.end(); ++it) {
            if (it->get() == wrapper) {
                toplevels.erase(it);
                break;
            }
        }
    }
};

void test_destroy_during_fade() {
    MockCompositorBackend backend;
    auto wrapper_ptr = std::make_unique<MockToplevelWrapper>();
    MockToplevelWrapper* wrapper = wrapper_ptr.get();
    backend.toplevels.push_back(std::move(wrapper_ptr));
    backend.active_toplevel = wrapper;

    // Begin fade-out
    MockTimer timer{true, 15, wrapper};
    wrapper->fade_timer = &timer;
    wrapper->is_closing = true;
    wrapper->opacity = 0.85; // Mid-flight fade

    // Client disappears unexpectedly (e.g. SIGKILL)
    backend.destroy_toplevel(wrapper);

    // Assertions
    assert(timer.armed == false);
    assert(timer.user_data == nullptr);
    assert(backend.active_toplevel == nullptr);
    assert(backend.toplevels.empty());

    std::cout << "[PASS] destroy_during_fade" << std::endl;
}

void test_destroy_with_active_timer() {
    MockCompositorBackend backend;
    auto wrapper_ptr = std::make_unique<MockToplevelWrapper>();
    MockToplevelWrapper* wrapper = wrapper_ptr.get();
    backend.toplevels.push_back(std::move(wrapper_ptr));

    MockTimer timer{true, 15, wrapper};
    wrapper->fade_timer = &timer;

    backend.destroy_toplevel(wrapper);

    // Verify timer cannot dereference wrapper
    assert(timer.armed == false);
    assert(timer.user_data == nullptr);
    assert(wrapper->fade_timer == nullptr);

    std::cout << "[PASS] destroy_with_active_timer" << std::endl;
}

void test_destroy_while_grabbed() {
    MockCompositorBackend backend;
    auto wrapper_ptr = std::make_unique<MockToplevelWrapper>();
    MockToplevelWrapper* wrapper = wrapper_ptr.get();
    backend.toplevels.push_back(std::move(wrapper_ptr));

    // Simulate user dragging or resizing the window
    backend.grabbed_toplevel = wrapper;
    backend.cursor_mode = MockCursorMode::Move;

    // Client terminates while grabbed
    backend.destroy_toplevel(wrapper);

    // Assert grab is completely released and cursor mode resets
    assert(backend.grabbed_toplevel == nullptr);
    assert(backend.cursor_mode == MockCursorMode::Passthrough);

    std::cout << "[PASS] destroy_while_grabbed" << std::endl;
}

void test_destroy_cleanup_is_idempotent() {
    MockCompositorBackend backend;
    auto wrapper_ptr = std::make_unique<MockToplevelWrapper>();
    MockToplevelWrapper* wrapper = wrapper_ptr.get();
    backend.toplevels.push_back(std::move(wrapper_ptr));

    MockTimer timer{true, 15, wrapper};
    wrapper->fade_timer = &timer;
    backend.grabbed_toplevel = wrapper;
    backend.cursor_mode = MockCursorMode::Resize;

    // First destruction call
    backend.destroy_toplevel(wrapper);
    assert(backend.toplevels.empty());
    assert(backend.grabbed_toplevel == nullptr);

    // Second destruction call on already destroyed wrapper (must be safe no-op)
    backend.destroy_toplevel(wrapper);
    assert(backend.grabbed_toplevel == nullptr);
    assert(backend.cursor_mode == MockCursorMode::Passthrough);

    std::cout << "[PASS] destroy_cleanup_is_idempotent" << std::endl;
}

int main() {
    test_destroy_during_fade();
    test_destroy_with_active_timer();
    test_destroy_while_grabbed();
    test_destroy_cleanup_is_idempotent();
    return 0;
}
