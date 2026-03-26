#include <ui/kui/cpp/fb.hpp>

extern "C" {
    void fb_putpixel(int x, int y, unsigned int color);
    int fb_get_width(void);
    int fb_get_height(void);
    void fb_present(void);
}

namespace kui {

void Framebuffer::putPixel(int x, int y, uint32_t color) {
    fb_putpixel(x, y, color);
}

int Framebuffer::width() const {
    return fb_get_width();
}

int Framebuffer::height() const {
    return fb_get_height();
}

void Framebuffer::present() {
    fb_present();
}

} // namespace kui