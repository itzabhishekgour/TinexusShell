#include "comp/animation/animation_manager.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <atomic>
#include <cmath>

using namespace tinexus::comp;

// 1. Animation starts & properties initialized
void test_animation_starts_correctly() {
    auto& mgr = AnimationManager::instance();
    mgr.cancel_all();
    assert(!mgr.has_active_animations());
    assert(mgr.active_animation_count() == 0);

    bool frame_scheduled = false;
    mgr.set_frame_scheduler([&frame_scheduled]() {
        frame_scheduled = true;
    });

    WindowAnimation anim;
    anim.window_id = 101;
    anim.type = WindowAnimationType::Maximize;
    anim.curve = AnimationCurve::EaseDecelerate;
    anim.duration_sec = 0.200;
    anim.start_geom = WindowBox{100, 100, 800, 600};
    anim.target_geom = WindowBox{0, 0, 1920, 1080};
    anim.start_opacity = 0.5;
    anim.target_opacity = 1.0;
    anim.start_scale = 0.8;
    anim.target_scale = 1.0;

    mgr.start_animation(anim);

    assert(frame_scheduled);
    assert(mgr.has_active_animations());
    assert(mgr.has_animation(101));
    assert(mgr.active_animation_count() == 1);

    const auto* running = mgr.get_animation(101);
    assert(running != nullptr);
    assert(running->window_id == 101);
    assert(running->current_geom.x == 100);
    assert(std::abs(running->current_opacity - 0.5) < 1e-4);
    assert(std::abs(running->current_scale - 0.8) < 1e-4);

    mgr.set_frame_scheduler(nullptr);
    std::cout << "[PASS] test_animation_starts_correctly" << std::endl;
}

// 2. Animation progresses with arbitrary dt and finishes exactly at target
void test_progresses_and_completes_exactly() {
    auto& mgr = AnimationManager::instance();
    mgr.cancel_all();
    mgr.set_frame_scheduler(nullptr);

    bool completed = false;
    std::vector<WindowBox> steps;

    WindowAnimation anim;
    anim.window_id = 202;
    anim.type = WindowAnimationType::Snap;
    anim.curve = AnimationCurve::Linear;
    anim.duration_sec = 0.100; // 100ms
    anim.start_geom = WindowBox{0, 0, 800, 600};
    anim.target_geom = WindowBox{0, 0, 960, 1080};
    anim.start_opacity = 1.0;
    anim.target_opacity = 1.0;
    anim.on_step = [&steps](const WindowAnimation& a) {
        steps.push_back(a.current_geom);
    };
    anim.on_complete = [&completed](const WindowAnimation& a) {
        completed = true;
        assert(a.current_geom.width == 960);
        assert(a.current_geom.height == 1080);
    };

    mgr.start_animation(anim);

    // Tick 50ms (halfway)
    mgr.tick(0.050);
    assert(mgr.has_active_animations());
    assert(!completed);
    assert(!steps.empty());

    const auto* mid = mgr.get_animation(202);
    assert(mid != nullptr);
    // Linear interpolation at 50%
    assert(std::abs(mid->current_geom.width - 880) <= 2);
    assert(std::abs(mid->current_geom.height - 840) <= 2);

    // Tick remaining 60ms (over duration -> snap to exact final state)
    mgr.tick(0.060);
    assert(completed);
    assert(!mgr.has_active_animations());
    assert(mgr.active_animation_count() == 0);

    std::cout << "[PASS] test_progresses_and_completes_exactly" << std::endl;
}

// 3. Zero-dt safety & Large-dt safety
void test_zero_dt_and_large_dt_safety() {
    auto& mgr = AnimationManager::instance();
    mgr.cancel_all();

    bool completed = false;
    WindowAnimation anim;
    anim.window_id = 303;
    anim.duration_sec = 0.150;
    anim.start_geom = WindowBox{10, 10, 100, 100};
    anim.target_geom = WindowBox{20, 20, 200, 200};
    anim.on_complete = [&completed](const WindowAnimation&) { completed = true; };

    mgr.start_animation(anim);

    // Zero dt: must not crash, must not finish prematurely
    mgr.tick(0.0);
    assert(!completed);
    assert(mgr.has_active_animations());

    // Negative dt: must be safely ignored
    mgr.tick(-0.016);
    assert(!completed);

    // Very large dt (e.g. system suspended or dropped frame): must complete cleanly without overshoot
    mgr.tick(10.0);
    assert(completed);
    assert(!mgr.has_active_animations());

    std::cout << "[PASS] test_zero_dt_and_large_dt_safety" << std::endl;
}

// 4. Cancellation test
void test_animation_cancellation() {
    auto& mgr = AnimationManager::instance();
    mgr.cancel_all();

    bool completed = false;
    WindowAnimation anim;
    anim.window_id = 404;
    anim.duration_sec = 0.500;
    anim.on_complete = [&completed](const WindowAnimation&) { completed = true; };

    mgr.start_animation(anim);
    assert(mgr.has_animation(404));

    mgr.tick(0.100);
    assert(mgr.has_animation(404));

    mgr.cancel_animation(404);
    assert(!mgr.has_animation(404));
    assert(!mgr.has_active_animations());

    mgr.tick(0.500);
    assert(!completed); // Cancelled animation must never call on_complete

    std::cout << "[PASS] test_animation_cancellation" << std::endl;
}

// 5. Client destruction during animation safety (F-03 safety)
void test_destroy_during_animation_safety() {
    auto& mgr = AnimationManager::instance();
    mgr.cancel_all();

    struct DummyClient {
        int value{42};
        bool destroyed{false};
    };

    auto* client = new DummyClient();
    uint64_t client_id = reinterpret_cast<uint64_t>(client);

    bool callback_executed_after_destroy = false;

    WindowAnimation anim;
    anim.window_id = client_id;
    anim.duration_sec = 0.300;
    anim.on_step = [client, &callback_executed_after_destroy](const WindowAnimation&) {
        if (client->destroyed) {
            callback_executed_after_destroy = true;
        }
    };
    anim.on_complete = [client, &callback_executed_after_destroy](const WindowAnimation&) {
        if (client->destroyed) {
            callback_executed_after_destroy = true;
        }
    };

    mgr.start_animation(anim);
    mgr.tick(0.050);

    // Simulate SIGKILL: client freed immediately
    client->destroyed = true;
    mgr.cancel_animation(client_id);
    delete client;

    // Further ticks must NOT touch the destroyed client
    mgr.tick(0.300);
    assert(!callback_executed_after_destroy);
    assert(!mgr.has_active_animations());

    std::cout << "[PASS] test_destroy_during_animation_safety" << std::endl;
}

// 6. Multiple simultaneous animations
void test_multiple_simultaneous_animations() {
    auto& mgr = AnimationManager::instance();
    mgr.cancel_all();

    int completed_count = 0;
    for (uint64_t id = 1; id <= 5; ++id) {
        WindowAnimation anim;
        anim.window_id = id;
        anim.duration_sec = 0.050 * static_cast<double>(id); // 50ms, 100ms, 150ms, 200ms, 250ms
        anim.on_complete = [&completed_count](const WindowAnimation&) {
            completed_count++;
        };
        mgr.start_animation(anim);
    }

    assert(mgr.active_animation_count() == 5);

    // After 75ms: id=1 should be complete (50ms), others running
    mgr.tick(0.075);
    assert(completed_count == 1);
    assert(mgr.active_animation_count() == 4);

    // After another 100ms (total 175ms): id=2 and id=3 complete
    mgr.tick(0.100);
    assert(completed_count == 3);
    assert(mgr.active_animation_count() == 2);

    // After another 100ms (total 275ms): all complete
    mgr.tick(0.100);
    assert(completed_count == 5);
    assert(mgr.active_animation_count() == 0);
    assert(!mgr.has_active_animations());

    std::cout << "[PASS] test_multiple_simultaneous_animations" << std::endl;
}

// 7. Different simulated refresh rates (60Hz, 75Hz, 90Hz, 120Hz, 144Hz)
void test_simulated_refresh_rates() {
    const std::vector<double> refresh_rates = {60.0, 75.0, 90.0, 120.0, 144.0, 165.0, 240.0};

    for (double hz : refresh_rates) {
        auto& mgr = AnimationManager::instance();
        mgr.cancel_all();

        const double dt = 1.0 / hz;
        const double duration = 0.150; // 150ms transition
        bool finished = false;

        WindowAnimation anim;
        anim.window_id = 999;
        anim.duration_sec = duration;
        anim.start_geom = WindowBox{0, 0, 100, 100};
        anim.target_geom = WindowBox{0, 0, 500, 500};
        anim.on_complete = [&finished](const WindowAnimation& a) {
            finished = true;
            assert(a.current_geom.width == 500);
        };

        mgr.start_animation(anim);

        int frames = 0;
        while (!finished && frames < 500) {
            mgr.tick(dt);
            frames++;
        }

        assert(finished);
        int expected_frames = static_cast<int>(std::ceil(duration / dt));
        assert(std::abs(frames - expected_frames) <= 2);
    }

    std::cout << "[PASS] test_simulated_refresh_rates (60Hz..240Hz)" << std::endl;
}

// 8. Idle frame suppression
void test_frame_suppression_on_idle() {
    auto& mgr = AnimationManager::instance();
    mgr.cancel_all();
    assert(!mgr.has_active_animations());

    // When no animations exist, ticking does nothing and has_active_animations remains false
    mgr.tick(0.016);
    assert(!mgr.has_active_animations());
    assert(mgr.active_animation_count() == 0);

    std::cout << "[PASS] test_frame_suppression_on_idle" << std::endl;
}

int main() {
    std::cout << "=== Running AnimationManager Tests ===" << std::endl;
    test_animation_starts_correctly();
    test_progresses_and_completes_exactly();
    test_zero_dt_and_large_dt_safety();
    test_animation_cancellation();
    test_destroy_during_animation_safety();
    test_multiple_simultaneous_animations();
    test_simulated_refresh_rates();
    test_frame_suppression_on_idle();
    std::cout << "=== All AnimationManager Tests PASSED ===" << std::endl;
    return 0;
}
