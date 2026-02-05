#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <kernel/drivers/video/fb.h> // fb_color_t tanımı buradan geliyor

/**
 * KuvixOS Grafik Kütüphanesi (GFX)
 * - Framebuffer (fb.c) katmanının üzerine inşa edilmiştir.
 * - ARGB (32-bit) renk paletini destekler.
 */

// Bitmap yapısı (Modern 32-bit ARGB destekli)
typedef struct {
    int width;
    int height;
    const uint32_t* pixels; // 0xAARRGGBB formatında dizi
} bitmap_t;

// --- Temel Başlatma ---
void gfx_init(void);

// --- Temizleme ve Temel Çizim ---
void gfx_clear(uint32_t color);
void gfx_putpixel(int x, int y, uint32_t color);

// İçi Dolu Dikdörtgen
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);

// Sadece Kenarlık (Yeni eklediğimiz fonksiyon)
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);

// Çizgi Çizimi
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color);

// Şeffaf (Alpha Blending) Dikdörtgen
void gfx_draw_alpha_rect(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int x, int y);

// --- Gelişmiş Şekiller ---
void gfx_fill_round_rect(int x, int y, int w, int h, int r, uint32_t color);
void gfx_draw_bitmap(int x, int y, const bitmap_t* bmp);

// --- Metin Çizimi ---
void gfx_draw_text(int x, int y, uint32_t color, const char* s);

#endif