#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <ui/font8x8_basic.h>

void gfx_init(void) {
    // Gelecekte hızlandırma veya buffer işlemleri buraya gelebilir
}

// Tüm ekranı bir ARGB rengiyle temizler
void gfx_clear(uint32_t color) {
    fb_clear(color);
}

// Temel piksel çizimi
void gfx_putpixel(int x, int y, uint32_t color) {
    fb_putpixel(x, y, color);
}

// Kare/Dikdörtgen çizimi
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    fb_draw_rect(x, y, w, h, color);
}

// Metin çizimi (8x8 font kullanarak)
void gfx_draw_text(int x, int y, uint32_t color, const char* s) {    
    if (!s) return;
    
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        const uint8_t* glyph = font8x8_basic[c];

        for (int row = 0; row < 8; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++) {
                // Bit 1 ise pikseli bas
                if (line & (1u << (7 - col))) {
                    fb_putpixel(x + col, y + row, color);
                }
            }
        }
        x += 8; // Bir sonraki karakter için 8 piksel sağa kay
    }
}

// Karekök fonksiyonu (Yuvarlak köşeler için yardımcı)
static uint32_t isqrt_u32(uint32_t n) {
    uint32_t x = n;
    uint32_t y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }
    return x;
}

// Modern UI için Yuvarlatılmış Köşeli Dikdörtgen (Filled)
void gfx_fill_round_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    if (r < 0) r = 0;
    int maxr = (w < h ? w : h) / 2;
    if (r > maxr) r = maxr;

    const int rr = r * r;

    for (int yy = 0; yy < h; yy++) {
        int inset = 0;
        if (r > 0) {
            if (yy < r) {
                int dy = (r - 1) - yy;
                inset = r - (int)isqrt_u32(rr - dy * dy);
            } else if (yy >= h - r) {
                int dy = yy - (h - r);
                inset = r - (int)isqrt_u32(rr - dy * dy);
            }
        }

        int x0 = x + inset;
        int x1 = x + w - inset - 1;
        for (int xx = x0; xx <= x1; xx++) {
            fb_putpixel(xx, y + yy, color);
        }
    }
}