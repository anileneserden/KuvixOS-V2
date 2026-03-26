#pragma once
#include <stdint.h>
#include <ui/kui/cpp/fb.hpp>
#include <ui/kui/cpp/color.hpp>

namespace kui {

class Graphics {
public:
    explicit Graphics(Framebuffer& fb);

    void putPixel(int x, int y, uint32_t color);
    void fillRect(int x, int y, int w, int h, uint32_t color);
    void clear(uint32_t color);

    void drawTextUtf8(int x, int y, uint32_t color, const char* text);

private:
    Framebuffer& m_fb;
};

} // namespace kui