#include <txui/layout/Constraints.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/layout/Padding.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <iostream>

using namespace txui;

void assert_eq(float64 actual, float64 expected, const char* msg) {
    if (std::abs(actual - expected) > 1e-6) {
        std::cerr << "FAIL: " << msg << " | Expected " << expected << " got " << actual << "\n";
        exit(1);
    }
}

void test_constraints() {
    Constraints c(10, 100, 20, 200);
    assert_eq(c.constrain(Size(5, 5)).width, 10, "constrain min_w");
    assert_eq(c.constrain(Size(5, 5)).height, 20, "constrain min_h");
    assert_eq(c.constrain(Size(500, 500)).width, 100, "constrain max_w");
    assert_eq(c.constrain(Size(500, 500)).height, 200, "constrain max_h");
    assert_eq(c.constrain(Size(50, 50)).width, 50, "constrain inside_w");
    assert_eq(c.constrain(Size(50, 50)).height, 50, "constrain inside_h");

    auto t = Constraints::tight(50, 50);
    if (!t.is_tight()) {
        std::cerr << "FAIL: tight constraint not tight\n";
        exit(1);
    }
}

void test_row_flex() {
    Ref<Row> row = make_ref<Row>();
    
    // Non-flexible items
    row->add_child(make_ref<SizedBox>(100, 40));
    row->add_child(make_ref<SizedBox>(50, 20));

    // Flexible items
    auto expanded = FlexItem::Expanded(make_ref<SizedBox>(0, 30), 2);
    auto flexible = FlexItem::Flexible(make_ref<SizedBox>(10, 50), 1);
    
    row->add_child(expanded);
    row->add_child(flexible);

    // Give it tight width 500 but loose height up to 100
    row->measure(Constraints(500, 500, 0, 100));
    row->layout(Rect(0, 0, 500, 50));

    // Non-flexible take 150 width. Remaining is 350.
    // Total flex is 3. Space per flex is 350 / 3 = 116.666...
    // Expanded takes 2 * 116.666 = 233.333
    // Flexible takes min(10, 116.666) = 10 (Wait, SizedBox returns its width (10), Flexible constraints let it be anything between 0 and 116.666, so it's 10!)
    // Wait, let's test sizes:
    assert_eq(row->children()[0]->desired_size().width, 100, "box1 w");
    assert_eq(row->children()[1]->desired_size().width, 50, "box2 w");
    assert_eq(row->children()[2]->desired_size().width, 350.0 * 2.0 / 3.0, "expanded w");
    assert_eq(row->children()[3]->desired_size().width, 10.0, "flexible w");
    
    // Max cross size
    assert_eq(row->desired_size().height, 50, "row height is max of cross sizes");
}

int main() {
    std::cout << "Running Layout Math tests...\n";
    test_constraints();
    test_row_flex();
    std::cout << "All layout tests passed!\n";
    return 0;
}
