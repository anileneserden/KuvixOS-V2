#ifndef KUVIX_FRAMEBUFFER_HPP
#define KUVIX_FRAMEBUFFER_HPP

#include <stdint.h>

extern "C" {
    #include <kernel/drivers/video/fb.h>
}

namespace Kuvix {

class Framebuffer {
public:
    static void clear(uint32_t color);
    static void present();
    static void presentRect(int x, int y, int w, int h);
    static uint32_t getWidth();
    static uint32_t getHeight();
    
    // Renk oluşturucu (fb_rgb wrapper)
    static uint32_t color(uint8_t r, uint8_t g, uint8_t b);
};

} // namespace Kuvix

#endif