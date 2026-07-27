#include <cassert>
#include <iostream>
#include "comp/render/damage_tracker.hpp"

using namespace tinexus::comp;

int main() {
    std::cout << "[+] Running smoke_damage_tracker test suite..." << std::endl;

    auto& tracker = DamageTracker::instance();
    tracker.set_screen_dimensions(1920, 1080);
    tracker.clear();

    // 1. Initial State Verification
    assert(tracker.has_damage() == false);
    assert(tracker.current_damage().empty() == true);

    // 2. Add Damage Boxes & Multi-Rect Merging
    DamageBox box1{100, 100, 200, 150};
    DamageBox box2{250, 150, 200, 150}; // Overlaps box1
    DamageBox box3{800, 600, 100, 100}; // Disjoint box

    tracker.add_damage(box1);
    assert(tracker.has_damage() == true);
    assert(tracker.current_damage().boxes().size() == 1);

    tracker.add_damage(box2);
    // Box1 and Box2 overlap, so DamageRegion should merge them into a single bounding box
    assert(tracker.current_damage().boxes().size() == 1);
    DamageBox merged = tracker.current_damage().boxes()[0];
    assert(merged.x == 100 && merged.y == 100 && merged.width == 350 && merged.height == 200);

    tracker.add_damage(box3);
    // Disjoint box3 should result in 2 distinct damage boxes in region
    assert(tracker.current_damage().boxes().size() == 2);

    // 3. Accumulate Damage Frame Ring Buffer
    tracker.accumulate_frame_damage();
    assert(tracker.accumulated_damage().boxes().size() >= 1);

    // 4. Full-Screen Fallback Threshold Verification (>80% area)
    tracker.clear();
    DamageBox huge_box{0, 0, 1800, 1000}; // ~86.8% screen area
    tracker.add_damage(huge_box);
    assert(tracker.current_damage().boxes().size() == 1);
    DamageBox fallback_box = tracker.current_damage().boxes()[0];
    assert(fallback_box.x == 0 && fallback_box.y == 0 && fallback_box.width == 1920 && fallback_box.height == 1080);

    std::cout << "[+] smoke_damage_tracker: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
