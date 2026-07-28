#pragma once

#include "painter.hpp"
#include "geometry.hpp"

namespace ui {

class Widget {
public:
    virtual ~Widget() = default;

    virtual void layout(int width, int height) = 0;
    virtual void draw(Painter& painter) = 0;
};

} // namespace ui
