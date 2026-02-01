// kernel/ui/cursor.c
#include <kernel/drivers/video/fb.h>
#include <ui/cursor.h>

// Python ile üretilen header dosyaları
#include "bitmaps/cursors/cursor_standart_arrow.h"
#include "bitmaps/cursors/cursor_resize_nwse.h"
#include "bitmaps/cursors/cursor_resize_nesw.h"
#include "bitmaps/cursors/cursor_resize_ns.h"
#include "bitmaps/cursors/cursor_resize_we.h"

static const fb_color_t cursor_palette[] = {
    0x00000000, // 0 = Transparent (Şeffaf)
    0xFFFFFFFF, // 2 = Beyaz (İç dolgu için)
    0xFF000000, // 1 = Siyah (Kenar çizgileri için)
};

void cursor_draw_generic(int x, int y, int w, int h, const uint8_t* bitmap) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t idx = bitmap[row * w + col];
            if (idx == 0) continue; 
            fb_putpixel(x + col, y + row, cursor_palette[idx]);
        }
    }
}

void cursor_draw_arrow(int x, int y) {
    // Döngü sınırlarını yeni boyuta (24) çekiyoruz
    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 24; col++) {
            // Direkt 2D erişim: cursor_clean[24][24]
            uint8_t idx = cursor_clean[row][col];
            
            if (idx == 0) continue; 
            
            fb_putpixel(x + col, y + row, cursor_palette[idx]);
        }
    }
}

// Çapraz Resize 1 (NW-SE)
void cursor_draw_resize_nwse(int x, int y) {
    cursor_draw_generic(x - 24, y - 24, 48, 48, (const uint8_t*)cursor_resize_nwse);
}

// Yatay Resize (W-E)
void cursor_draw_resize_we(int x, int y) {
    cursor_draw_generic(x - 24, y - 24, 48, 48, (const uint8_t*)cursor_resize_we);
}

// Dikey Resize (N-S) -> EKSİKTİ, EKLENDİ
void cursor_draw_resize_ns(int x, int y) {
    cursor_draw_generic(x - 24, y - 24, 48, 48, (const uint8_t*)cursor_resize_ns);
}

// Çapraz Resize 2 (NE-SW) -> EKSİKTİ, EKLENDİ
void cursor_draw_resize_nesw(int x, int y) {
    cursor_draw_generic(x - 24, y - 24, 48, 48, (const uint8_t*)cursor_resize_nesw);
}