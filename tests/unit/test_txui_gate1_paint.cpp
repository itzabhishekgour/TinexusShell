#include <txui/widgets/Widget.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/layout/Padding.hpp>
#include <txui/render/Painter.hpp>
#include <iostream>
#include <cstdint>

using namespace txui;

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
    void add_uint8(uint8_t v) { add(&v, sizeof(v)); }
    
    void add_size(const Size& s) {
        add_float64(s.width);
        add_float64(s.height);
    }
    
    void add_rect(const Rect& r) {
        add_float64(r.origin.x);
        add_float64(r.origin.y);
        add_size(r.size);
    }

    void add_color(const Color& c) {
        add_float64(c.r());
        add_float64(c.g());
        add_float64(c.b());
        add_float64(c.a());
    }

    uint64_t get() const { return hash; }
};

struct PaintHashVisitor {
    HashBuilder builder;

    void operator()(const BeginFrameCommand&) { builder.add_uint8(1); }
    void operator()(const EndFrameCommand&) { builder.add_uint8(2); }
    
    void operator()(const SetBrushCommand& cmd) {
        builder.add_uint8(3);
        if (std::holds_alternative<SolidBrush>(cmd.brush)) {
            builder.add_uint8(1);
            builder.add_color(std::get<SolidBrush>(cmd.brush).color());
        }
    }
    
    void operator()(const DrawRectCommand& cmd) {
        builder.add_uint8(4);
        builder.add_rect(cmd.rect);
        if (std::holds_alternative<SolidBrush>(cmd.brush)) {
            builder.add_color(std::get<SolidBrush>(cmd.brush).color());
        }
    }
    
    void operator()(const DrawRoundedRectCommand& cmd) {
        builder.add_uint8(5);
        builder.add_rect(cmd.rect);
        builder.add_float64(cmd.radius);
        if (std::holds_alternative<SolidBrush>(cmd.brush)) {
            builder.add_color(std::get<SolidBrush>(cmd.brush).color());
        }
    }

    template <typename T>
    void operator()(const T&) {
        builder.add_uint8(255);
    }
};

int main() {
    std::cout << "Running Gate 1: Paint Determinism Test...\n";

    auto root = make_ref<Column>();
    root->add_child(make_ref<SolidColorWidget>(Color(255, 0, 0, 255)));
    root->add_child(make_ref<SolidColorWidget>(Color(0, 255, 0, 255)));

    root->measure(Constraints::tight(800, 600));
    root->layout(Rect(0, 0, 800, 600));

    uint64_t baseline_hash = 0;

    for (int i = 0; i < 1000; ++i) {
        CommandBuffer buffer;
        Painter painter(buffer);
        
        painter.begin_frame();
        root->paint(painter);
        painter.end_frame();
        
        PaintHashVisitor visitor;
        for (const auto& cmd : buffer.commands()) {
            std::visit(visitor, cmd);
        }
        
        uint64_t current_hash = visitor.builder.get();
        
        if (i == 0) {
            baseline_hash = current_hash;
        } else {
            if (current_hash != baseline_hash) {
                std::cerr << "FAIL: Paint determinism broken at iteration " << i << "!\n";
                return 1;
            }
        }
    }

    std::cout << "PASS: 1000 iterations of paint generated identical hashes (hash=" << std::hex << baseline_hash << ")\n";
    return 0;
}
