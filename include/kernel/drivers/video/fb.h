#ifndef FB_H
#define FB_H

#include <stdint.h>

// 1. Tip Tanımlaması (Legacy dosyaların hata vermemesi için kritik)
typedef uint32_t fb_color_t;

// Standart 800x600 32bpp ayarları
#define FB_WIDTH  800
#define FB_HEIGHT 600
#define FB_BPP    32
#define fb_rgba(r, g, b, a) fb_rgb(r, g, b)

// 2. Fonksiyon Prototipleri
void fb_init(uint32_t vbe_lfb_addr);
void fb_putpixel(int x, int y, fb_color_t color);
void fb_draw_rect(int x, int y, int w, int h, fb_color_t color);
void fb_draw_rect_outline(int x, int y, int w, int h, fb_color_t color); // Window.c için gerekli
void fb_clear(fb_color_t color);
void fb_present(void);

uint32_t fb_get_width(void);
uint32_t fb_get_height(void);

// Renk yardımcıları
fb_color_t fb_rgb(uint8_t r, uint8_t g, uint8_t b);
fb_color_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a); // Blit işlemleri için

#endif