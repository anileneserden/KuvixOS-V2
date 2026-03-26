#include <ui/kui/cpp/graphics.hpp>

namespace kui {

Graphics::Graphics(Framebuffer& fb)
    : m_fb(fb) {
}

void Graphics::putPixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0) return;
    if (x >= m_fb.width() || y >= m_fb.height()) return;
    m_fb.putPixel(x, y, color);
}

void Graphics::fillRect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            putPixel(x + xx, y + yy, color);
        }
    }
}

void Graphics::clear(uint32_t color) {
    fillRect(0, 0, m_fb.width(), m_fb.height(), color);
}

} // namespace kui