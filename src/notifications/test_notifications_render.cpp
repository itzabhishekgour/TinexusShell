#include "notifications/NotificationBubble.hpp"
#include "notifications/NotificationStackWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <cassert>

using namespace tinexus;
using namespace tinexus::notifications;

static bool render_stack_to_png(txui::Ref<NotificationStackWidget>& stack,
                                uint32_t width, uint32_t height,
                                const std::string& filename) {
    txui::Canvas canvas(width, height);
    canvas.clear(txui::Color(14, 14, 20, 255)); // Dark background so alpha glow/shadow is clearly visible
    txui::PixmanBackend backend;

    stack->measure(txui::Constraints::tight(width, height));
    stack->layout(txui::Rect(0, 0, width, height));

    txui::CommandBuffer cmds;
    txui::Painter painter(cmds);
    painter.begin_frame();
    stack->paint(painter);
    painter.end_frame();
    backend.execute(cmds, canvas);

    if (!txui::ImageWriter::save_png(canvas, filename)) {
        std::cerr << "FAIL: Could not save " << filename << std::endl;
        return false;
    }
    std::cout << "[Visual Test] Successfully saved " << filename << std::endl;
    return true;
}

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "=== Tinexus Notifications Timing & Visual Test Suite ===" << std::endl;
    std::cout << "========================================================" << std::endl;

    // ─────────────────────────────────────────────────────────────────────────
    // PART 1: Programmatic Timing Verification (User Clarification #3)
    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 1] Verifying Critical Alert persistence vs Normal Auto-Dismiss (8s+ timing test)..." << std::endl;

    NotificationItem normal_item;
    normal_item.id = 1001;
    normal_item.app_name = "Mail";
    normal_item.summary = "New message from Linus";
    normal_item.body = "Merge window for v6.12 is now open.";
    normal_item.urgency = Urgency::Normal;
    normal_item.expire_timeout_ms = 4000; // 4.0 seconds timeout

    NotificationItem critical_item;
    critical_item.id = 1002;
    critical_item.app_name = "Power Management";
    critical_item.summary = "Critically Low Battery (5%)";
    critical_item.body = "Plug in AC adapter immediately to avoid shutdown.";
    critical_item.urgency = Urgency::Critical;
    critical_item.expire_timeout_ms = 0; // 0 = never auto-expire per spec

    auto normal_bubble = txui::make_ref<NotificationBubble>(normal_item);
    auto critical_bubble = txui::make_ref<NotificationBubble>(critical_item);

    // Simulate stepping virtual clock in 100ms frames
    const std::chrono::milliseconds dt(100);
    double elapsed_s = 0.0;

    // t = 0 to 1.0s: Both bubbles slide in
    for (int step = 0; step < 10; ++step) {
        normal_bubble->update(dt);
        critical_bubble->update(dt);
        elapsed_s += 0.1;
    }

    assert(normal_bubble->state() == BubbleState::Visible);
    assert(critical_bubble->state() == BubbleState::Visible);
    std::cout << "  ✓ t=" << elapsed_s << "s: Both Normal and Critical bubbles slid in and reached Visible state." << std::endl;

    // t = 1.0s to 3.8s: Both remain Visible
    for (int step = 0; step < 28; ++step) {
        normal_bubble->update(dt);
        critical_bubble->update(dt);
        elapsed_s += 0.1;
    }
    assert(normal_bubble->state() == BubbleState::Visible);
    assert(critical_bubble->state() == BubbleState::Visible);
    std::cout << "  ✓ t=" << elapsed_s << "s: Both bubbles still active and visible." << std::endl;

    // t = 3.9s to 6.0s: Normal bubble times out (4.0s) and slides out to Hidden
    for (int step = 0; step < 22; ++step) {
        normal_bubble->update(dt);
        critical_bubble->update(dt);
        elapsed_s += 0.1;
    }
    assert(normal_bubble->state() == BubbleState::Hidden);
    assert(critical_bubble->state() == BubbleState::Visible);
    std::cout << "  ✓ t=" << elapsed_s << "s: Normal bubble timed out and transitioned to Hidden." << std::endl;
    std::cout << "  ✓ t=" << elapsed_s << "s: Critical alert persisted in Visible state (no auto-dismiss)." << std::endl;

    // t = 6.0s to 12.0s: Continue stepping past 8s, 10s, 12s
    for (int step = 0; step < 60; ++step) {
        normal_bubble->update(dt);
        critical_bubble->update(dt);
        elapsed_s += 0.1;
    }
    assert(normal_bubble->state() == BubbleState::Hidden);
    assert(critical_bubble->state() == BubbleState::Visible);
    assert(critical_bubble->offset_x() == 0.0);
    assert(critical_bubble->opacity() == 1.0);
    std::cout << "  ✓ t=" << elapsed_s << "s: Passed 12.0s virtual time! Critical alert strictly persisted." << std::endl;

    // Test explicit user dismissal on critical alert
    critical_bubble->dismiss();
    for (int step = 0; step < 10; ++step) {
        critical_bubble->update(dt);
        elapsed_s += 0.1;
    }
    assert(critical_bubble->state() == BubbleState::Hidden);
    std::cout << "  ✓ t=" << elapsed_s << "s: Critical alert cleanly slides out and dismisses upon explicit user interaction." << std::endl;

    std::cout << "[PASS] Test 1: Programmatic timing verification successful!\n" << std::endl;

    // ─────────────────────────────────────────────────────────────────────────
    // PART 2: Visual Rendering Suite
    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "[TEST 2] Generating visual artifact renders..." << std::endl;

    // 1. Single Normal Notification Render
    {
        auto stack = txui::make_ref<NotificationStackWidget>();
        NotificationItem item;
        item.id = 2001;
        item.app_name = "Calendar";
        item.summary = "Design Team Sync in 10 minutes";
        item.body = "Conference Room 4B / Video link attached.";
        item.urgency = Urgency::Normal;
        item.expire_timeout_ms = 6000;
        stack->add_bubble(item);

        // Advance 400ms to complete slide-in animation
        for (int i = 0; i < 5; ++i) stack->update(std::chrono::milliseconds(100));

        if (!render_stack_to_png(stack, 420, 140, "notifications_single_normal.png")) return 1;
    }

    // 2. Stacked Notifications Render (3 bubbles)
    {
        auto stack = txui::make_ref<NotificationStackWidget>();

        NotificationItem i1;
        i1.id = 3001;
        i1.app_name = "Pulse Launcher";
        i1.summary = "Application installed";
        i1.body = "Visual Studio Code is ready to launch.";
        i1.urgency = Urgency::Low;
        i1.expire_timeout_ms = 4000;
        stack->add_bubble(i1);

        NotificationItem i2;
        i2.id = 3002;
        i2.app_name = "Slack";
        i2.summary = "Sarah Connor";
        i2.body = "Can you review the PR for the compositor direct scanout?";
        i2.urgency = Urgency::Normal;
        i2.expire_timeout_ms = 6000;
        stack->add_bubble(i2);

        NotificationItem i3;
        i3.id = 3003;
        i3.app_name = "Software Update";
        i3.summary = "Tinexus Platform 1.2 Available";
        i3.body = "Includes Vulkan direct scanout and memory allocator fixes.";
        i3.urgency = Urgency::Normal;
        i3.expire_timeout_ms = 6000;
        stack->add_bubble(i3);

        for (int i = 0; i < 5; ++i) stack->update(std::chrono::milliseconds(100));

        if (!render_stack_to_png(stack, 420, 360, "notifications_stacked.png")) return 1;
    }

    // 3. Critical Alert Notification Render (with Action buttons)
    {
        auto stack = txui::make_ref<NotificationStackWidget>();

        NotificationItem item;
        item.id = 4001;
        item.app_name = "Security Subsystem";
        item.summary = "Untrusted Binary Execution Attempted";
        item.body = "/tmp/miner failed cryptographic signature check (Ed25519).";
        item.urgency = Urgency::Critical;
        item.expire_timeout_ms = 0;
        item.actions.push_back({"block", "Quarantine"});
        item.actions.push_back({"details", "View Audit"});
        stack->add_bubble(item);

        for (int i = 0; i < 5; ++i) stack->update(std::chrono::milliseconds(100));

        if (!render_stack_to_png(stack, 420, 180, "notifications_critical.png")) return 1;
    }

    std::cout << "=== All Notification center visual and timing tests completed successfully! ===" << std::endl;
    return 0;
}
