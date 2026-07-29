#include <txui/widgets/Widget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <txui/layout/Padding.hpp>
#include <iostream>
#include <cstdint>
#include <vector>

using namespace txui;

// Simple FNV-1a hash for deterministic state verification
class HashBuilder {
private:
    uint64_t hash = 14695981039346656037ull;

public:
    void add(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= ptr[i];
            hash *= 1099511628211ull;
        }
    }

    void add_float64(float64 v) { add(&v, sizeof(v)); }
    void add_uint32(uint32_t v) { add(&v, sizeof(v)); }
    
    void add_size(const Size& s) {
        add_float64(s.width);
        add_float64(s.height);
    }
    
    void add_rect(const Rect& r) {
        add_float64(r.origin.x);
        add_float64(r.origin.y);
        add_size(r.size);
    }
    
    void add_constraints(const Constraints& c) {
        add_float64(c.min_width);
        add_float64(c.max_width);
        add_float64(c.min_height);
        add_float64(c.max_height);
    }

    uint64_t get() const { return hash; }
};

void hash_widget_tree(const Widget* widget, HashBuilder& builder, uint32_t depth = 0) {
    if (!widget) return;
    
    builder.add_uint32(depth);
    builder.add_rect(widget->frame());
    builder.add_size(widget->desired_size());
    // Constraints are transiently passed to measure(), but we can assume desired_size reflects it.
    
    for (const auto& child : widget->children()) {
        hash_widget_tree(child.get(), builder, depth + 1);
    }
}

int main() {
    std::cout << "Running Gate 1: Layout Determinism Test...\n";

    auto root = make_ref<Column>();
    root->set_main_axis_alignment(MainAxisAlignment::SpaceBetween);
    
    auto row = make_ref<Row>();
    row->add_child(make_ref<SizedBox>(100, 50));
    row->add_child(FlexItem::Expanded(make_ref<SizedBox>(0, 50), 2));
    
    auto padded_row = make_ref<Padding>(Insets(10, 20), row);
    
    root->add_child(padded_row);
    root->add_child(FlexItem::Expanded(make_ref<SizedBox>(0, 0), 1));
    
    uint64_t baseline_hash = 0;

    for (int i = 0; i < 1000; ++i) {
        root->mark_needs_measure();
        root->measure(Constraints::tight(800, 600));
        root->layout(Rect(0, 0, 800, 600));
        
        HashBuilder builder;
        hash_widget_tree(root.get(), builder);
        uint64_t current_hash = builder.get();
        
        if (i == 0) {
            baseline_hash = current_hash;
        } else {
            if (current_hash != baseline_hash) {
                std::cerr << "FAIL: Layout determinism broken at iteration " << i << "!\n";
                return 1;
            }
        }
    }

    std::cout << "PASS: 1000 iterations of layout generated identical hashes (hash=" << std::hex << baseline_hash << ")\n";
    return 0;
}
