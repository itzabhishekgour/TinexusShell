#include <cassert>
#include <iostream>
#include "comp/output/output_layout.hpp"

using namespace tinexus::comp;

class TestObserver : public IOutputObserver {
public:
    uint32_t added_count{0};
    uint32_t removed_count{0};

    void on_output_added(const OutputSpec&) override { added_count++; }
    void on_output_removed(const std::string&) override { removed_count++; }
    void on_mode_changed(const std::string&, uint32_t, uint32_t, uint32_t) override {}
    void on_output_enabled(const std::string&) override {}
    void on_output_disabled(const std::string&) override {}
    void on_scale_changed(const std::string&, float) override {}
    void on_transform_changed(const std::string&, int32_t) override {}
};

int main() {
    std::cout << "[+] Running smoke_output_layout test suite..." << std::endl;

    auto& layout = OutputLayout::instance();
    TestObserver observer;
    layout.register_observer(&observer);

    // 1. Add Primary Display (HDMI-A-1 at 0,0)
    OutputSpec primary_spec{"HDMI-A-1", 0, 0, 1920, 1080, 60, 1.0f, 0, true, true};
    layout.add_output(primary_spec);

    assert(observer.added_count == 1);
    assert(layout.outputs().size() == 1);
    assert(layout.get_primary() != nullptr);
    assert(layout.get_primary()->name == "HDMI-A-1");

    // 2. Add Left-hand Display with Negative Coordinates (DP-1 at -1920, 0)
    OutputSpec left_spec{"DP-1", -1920, 0, 1920, 1080, 144, 1.0f, 0, true, false};
    layout.add_output(left_spec);

    assert(observer.added_count == 2);
    assert(layout.outputs().size() == 2);

    // 3. Point Lookup Verification
    const OutputSpec* out_left = layout.output_at_point(-500, 500);
    assert(out_left != nullptr);
    assert(out_left->name == "DP-1");

    const OutputSpec* out_primary = layout.output_at_point(500, 500);
    assert(out_primary != nullptr);
    assert(out_primary->name == "HDMI-A-1");

    // 4. Global ↔ Local Coordinate Transforms
    Point2D local_p = layout.global_to_output_coords("DP-1", -1000, 400);
    assert(local_p.x == 920); // -1000 - (-1920) = 920
    assert(local_p.y == 400);

    Point2D global_p = layout.output_to_global_coords("DP-1", 920, 400);
    assert(global_p.x == -1000);
    assert(global_p.y == 400);

    // 5. Observer Notification on Removal
    assert(layout.remove_output("DP-1") == true);
    assert(observer.removed_count == 1);
    assert(layout.outputs().size() == 1);

    layout.unregister_observer(&observer);
    std::cout << "[+] smoke_output_layout: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
