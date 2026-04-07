#include <cpp/Graphics.hpp>

namespace Kuvix {

void Graphics::fillRect(int x, int y, int w, int h, uint32_t color) {
    gfx_fill_rect(x, y, w, h, color);
}

void Graphics::drawRoundRect(int x, int y, int w, int h, int radius, uint32_t color) {
    gfx_fill_round_rect(x, y, w, h, radius, color);
}

// Parametre sırası hpp ile uyumlu hale getirildi: (x, y, text, color)
void Graphics::drawText(int x, int y, const char* text, uint32_t color) {
    gfx_draw_text(x, y, color, text);
}

void Graphics::clear(uint32_t color) {
    gfx_clear(color);
}

} // namespace Kuvix