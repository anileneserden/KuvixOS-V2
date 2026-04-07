#ifndef KUVIX_GRAPHICS_HPP
#define KUVIX_GRAPHICS_HPP

#include <stdint.h>

extern "C" {
    #include <kernel/drivers/video/gfx.h>
}

namespace Kuvix {

class Graphics {
public:
    static void fillRect(int x, int y, int w, int h, uint32_t color);
    static void drawRoundRect(int x, int y, int w, int h, int radius, uint32_t color);
    static void drawText(int x, int y, const char* text, uint32_t color);
    static void clear(uint32_t color);
    
    // Renk oluşturucu (ARGB)
    static uint32_t argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
        return (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
    }
};

} // namespace Kuvix

#endif