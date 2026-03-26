#pragma once
#include <stdint.h>

namespace kui {

class Framebuffer {
public:
    Framebuffer() = default;

    void putPixel(int x, int y, uint32_t color);
    int width() const;
    int height() const;
};

} // namespace kui