#include <txui/math/Point.hpp>
#include <txui/math/Size.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Insets.hpp>
#include <txui/math/Matrix3.hpp>
#include <txui/math/Transform.hpp>
#include <txui/math/ColorSpace.hpp>
#include <iostream>
#include <cstdlib>
#include <cmath>

int main() {
    txui::Rect r(10.0f, 20.0f, 100.0f, 50.0f);
    if (r.left() != 10.0f || r.top() != 20.0f || r.right() != 110.0f || r.bottom() != 70.0f) {
        std::cerr << "FAIL: Rect bounding coordinates incorrect" << std::endl;
        return EXIT_FAILURE;
    }

    if (!r.contains(15.0f, 25.0f)) {
        std::cerr << "FAIL: Rect contains interior point check failed" << std::endl;
        return EXIT_FAILURE;
    }

    if (r.contains(200.0f, 25.0f)) {
        std::cerr << "FAIL: Rect contains exterior point check failed" << std::endl;
        return EXIT_FAILURE;
    }

    txui::Transform transform;
    transform.translate(5.0f, 10.0f).scale(2.0f, 2.0f);
    txui::Point pt(10.0f, 10.0f);
    txui::Point res = transform.map(pt);
    if (std::abs(res.x - 25.0f) > 0.001f || std::abs(res.y - 30.0f) > 0.001f) {
        std::cerr << "FAIL: Transform map incorrect: got (" << res.x << ", " << res.y << ")" << std::endl;
        return EXIT_FAILURE;
    }

    txui::Insets insets(10.0f, 5.0f);
    if (insets.horizontal() != 10.0f || insets.vertical() != 20.0f) {
        std::cerr << "FAIL: Insets horizontal/vertical sum incorrect" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "PASS: test_txui_math passed successfully" << std::endl;
    return EXIT_SUCCESS;
}
