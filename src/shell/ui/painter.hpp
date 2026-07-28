#pragma once

#include "color.hpp"
#include "geometry.hpp"
#include "../render/surface_buffer.hpp"
#include <cstdint>

namespace ui {

class Painter {
public:
    virtual ~Painter() = default;

    virtual void begin(SurfaceBuffer* buffer) = 0;
    virtual void end() = 0;

    virtual void fill_rect(const Rect& rect, const Color& color) = 0;
    virtual void clear(const Color& color) = 0;
};

class ShmPainter : public Painter {
public:
    void begin(SurfaceBuffer* buffer) override;
    void end() override;

    void fill_rect(const Rect& rect, const Color& color) override;
    void clear(const Color& color) override;

private:
    SurfaceBuffer* m_buffer{nullptr};
};

} // namespace ui
