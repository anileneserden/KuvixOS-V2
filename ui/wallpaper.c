#include <ui/wallpaper.h>

const uint32_t g_wallpaper_w = 16;
const uint32_t g_wallpaper_h = 16;

static uint32_t g_wp[64*64];

__attribute__((constructor))
static void init_wp(void) {
    for (int y=0;y<64;y++){
        for (int x=0;x<64;x++){
            int v = ((x/8) ^ (y/8)) & 1;
            g_wp[y*64+x] = v ? 0xFF202A44 : 0xFF0E1426; // iki koyu ton
        }
    }
}

const uint32_t g_wallpaper_pixels[] = { 0 }; // bunu kullanmayacağız
