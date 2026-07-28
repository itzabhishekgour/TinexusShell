#pragma once

#include <cstdint>

class SurfaceBuffer {
public:
    virtual ~SurfaceBuffer() = default;

    // Returns a pointer to the buffer's pixels
    virtual uint32_t* pixels() = 0;

    // Returns the width of the buffer in pixels
    virtual int width() const = 0;

    // Returns the height of the buffer in pixels
    virtual int height() const = 0;

    // Signals that drawing is complete and the buffer can be attached/committed to a surface
    virtual void commit() = 0;
};
