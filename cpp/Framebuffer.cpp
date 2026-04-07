#include <cpp/Framebuffer.hpp>

namespace Kuvix {

void Framebuffer::clear(uint32_t color) {
    fb_clear(color);
}

void Framebuffer::present() {
    fb_present();
}

void Framebuffer::presentRect(int x, int y, int w, int h) {
    fb_present_rect(x, y, w, h);
}

uint32_t Framebuffer::getWidth() {
    return fb_get_width();
}

uint32_t Framebuffer::getHeight() {
    return fb_get_height();
}

uint32_t Framebuffer::color(uint8_t r, uint8_t g, uint8_t b) {
    return fb_rgb(r, g, b);
}

} // namespace Kuvix