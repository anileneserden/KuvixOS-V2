#include <kernel/drivers/video/fb.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

static uint32_t* fb_addr = 0;        // Gerçek Video Belleği (LFB)
static uint32_t* fb_backbuffer = 0;  // Titremeyi önleyen arka tampon

void fb_init(uint32_t vbe_lfb_addr) {
    fb_addr = (uint32_t*)vbe_lfb_addr;
    
    // Arka tampon için bellek ayır (800x600x4 byte)
    // KuvixOS'un kmalloc fonksiyonunu kullanıyoruz
    fb_backbuffer = (uint32_t*)kmalloc(FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));
    
    fb_clear(0x000000); // Ekranı siyahla başlat
    fb_present();       // İlk görüntüyü gönder
}

void fb_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    
    // Pikselleri doğrudan ekrana değil, arka tampona yazıyoruz
    fb_backbuffer[y * FB_WIDTH + x] = color;
}

void fb_clear(uint32_t color) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb_backbuffer[i] = color;
    }
}

void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            fb_putpixel(x + j, y + i, color);
        }
    }
}

void fb_present(void) {
    if (!fb_addr || !fb_backbuffer) return;
    
    // Arka tamponu gerçek video belleğine kopyala (Double Buffering)
    // Bu işlem titremeyi engeller
    memcpy(fb_addr, fb_backbuffer, FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));
}

uint32_t fb_get_width(void) { return FB_WIDTH; }
uint32_t fb_get_height(void) { return FB_HEIGHT; }

uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)((r << 16) | (g << 8) | b);
}

void fb_draw_rect_outline(int x, int y, int w, int h, fb_color_t color)
{
    if (w <= 0 || h <= 0) return;

    // Üst ve Alt kenarlar
    for (int xx = 0; xx < w; xx++) {
        fb_putpixel(x + xx, y,         color);
        fb_putpixel(x + xx, y + h - 1, color);
    }

    // Sol ve Sağ kenarlar
    for (int yy = 0; yy < h; yy++) {
        fb_putpixel(x,         y + yy, color);
        fb_putpixel(x + w - 1, y + yy, color);
    }
}