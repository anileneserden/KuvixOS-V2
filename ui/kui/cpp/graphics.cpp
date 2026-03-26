#include <ui/kui/cpp/graphics.hpp>

extern "C" {
    void gfx_clear(unsigned int color);
    void gfx_draw_text_utf8(int x, int y, unsigned int color, const char* text);
    void gfx_fill_round_rect(int x, int y, int w, int h, int radius, unsigned int color);
}

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

void Graphics::fillRoundRect(int x, int y, int w, int h, int radius, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (radius < 0) radius = 0;
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;

    gfx_fill_round_rect(x, y, w, h, radius, color);
}

void Graphics::clear(uint32_t color) {
    gfx_clear(color);
}

void Graphics::drawTextUtf8(int x, int y, uint32_t color, const char* text) {
    if (!text) return;
    gfx_draw_text_utf8(x, y, color, text);
}

void Graphics::present() {
    m_fb.present();
}

} // namespace kui