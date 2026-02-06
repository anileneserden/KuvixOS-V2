#ifndef FB_H
#define FB_H

#include <stdint.h>

// 1. Tip Tanımlaması
typedef uint32_t fb_color_t;

// Global çözünürlük değişkenleri
extern uint32_t FB_WIDTH;
extern uint32_t FB_HEIGHT;
extern uint32_t FB_PITCH;
#define FB_BPP    32

// 2. Fonksiyon Prototipleri
// kmain.c ile uyumlu 4 parametreli init
void fb_init(uint32_t vbe_lfb_addr, uint32_t width, uint32_t height, uint32_t pitch);

void fb_putpixel(int x, int y, fb_color_t color);
fb_color_t fb_getpixel(int x, int y);
void fb_draw_rect(int x, int y, int w, int h, fb_color_t color);
void fb_draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void fb_clear(fb_color_t color);
void fb_present(void);

uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pitch(void);

// UI ve Pencere yönetimi için gerekli blit fonksiyonu
void fb_blit_argb_key(int x, int y, int w, int h, const uint32_t* data, uint32_t key);

// Renk yardımcıları
fb_color_t fb_rgb(uint8_t r, uint8_t g, uint8_t b);
fb_color_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a); 

void fb_set_resolution(uint32_t width, uint32_t height);

#endif