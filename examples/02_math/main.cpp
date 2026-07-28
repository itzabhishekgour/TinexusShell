#include <txui/math/Point.hpp>
#include <txui/math/Size.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Transform.hpp>
#include <txui/core/Logger.hpp>
#include <iostream>

int main() {
    std::cout << "=== txui-demo-02-math ===" << std::endl;

    txui::Rect r1(10.0f, 20.0f, 100.0f, 50.0f);
    std::cout << "Rect r1: left=" << r1.left() << ", top=" << r1.top()
              << ", width=" << r1.width() << ", height=" << r1.height() << std::endl;

    txui::Point p_inside(50.0f, 30.0f);
    txui::Point p_outside(200.0f, 30.0f);

    std::cout << "contains(50, 30): " << (r1.contains(p_inside) ? "true" : "false") << std::endl;
    std::cout << "contains(200, 30): " << (r1.contains(p_outside) ? "true" : "false") << std::endl;

    txui::Transform transform;
    transform.translate(10.0f, 15.0f).scale(2.0f, 2.0f);

    txui::Point pt(5.0f, 5.0f);
    txui::Point mapped = transform.map(pt);
    std::cout << "Mapped (5,5) -> (" << mapped.x << ", " << mapped.y << ")" << std::endl;

    std::cout << "=== math demo completed successfully ===" << std::endl;
    return 0;
}
