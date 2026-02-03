#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <ui/font8x8_basic.h>
#include <stdint.h>

// kernel/drivers/video/gfx.c

static uint8_t tr_glyphs[12][16] = {
    // KÜÇÜK HARFLER (14 - 19)
    {0x7C, 0x00, 0x78, 0xC4, 0x84, 0x84, 0xC4, 0x7C, 0x04, 0x78, 0,0,0,0,0,0}, // 14: ğ
    {0x00, 0x00, 0x00, 0x3C, 0x40, 0x3C, 0x04, 0x3C, 0x00, 0x18, 0x0C, 0,0,0,0,0}, // 15: ş
    {0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0,0,0,0,0,0},       // 16: ı
    {0x44, 0x00, 0x3C, 0x44, 0x44, 0x44, 0x44, 0x3C, 0,0,0,0,0,0,0,0},             // 17: ö
    {0x00, 0x00, 0x00, 0x3C, 0x40, 0x40, 0x40, 0x3C, 0x00, 0x18, 0x0C, 0,0,0,0,0}, // 18: ç
    {0x44, 0x00, 0x44, 0x44, 0x44, 0x44, 0x44, 0x3E, 0,0,0,0,0,0,0,0},             // 19: ü

    // BÜYÜK HARFLER (20 - 25)
    {0x7C, 0x00, 0x3E, 0x40, 0x40, 0x47, 0x41, 0x3E, 0,0,0,0,0,0,0,0},             // 20: Ğ
    {0x3E, 0x40, 0x3C, 0x02, 0x3E, 0x00, 0x0C, 0x06, 0,0,0,0,0,0,0,0},             // 21: Ş
    {0x18, 0x00, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x3C, 0,0,0,0,0,0,0,0},             // 22: İ
    {0x66, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0,0,0,0,0,0,0,0},             // 23: Ö
    {0x3E, 0x40, 0x40, 0x40, 0x3E, 0x00, 0x0C, 0x06, 0,0,0,0,0,0,0,0},             // 24: Ç
    {0x66, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0,0,0,0,0,0,0,0}              // 25: Ü
};

void gfx_init(void) {
    fb_clear(0x000000); 
    fb_present();       
}

void gfx_clear(uint32_t color) {
    fb_clear(color);
}

void gfx_putpixel(int x, int y, uint32_t color) {
    fb_putpixel(x, y, color);
}

void gfx_putpixel_alpha(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || y < 0 || (uint32_t)x >= fb_get_width() || (uint32_t)y >= fb_get_height()) return;
    uint32_t bg_color = fb_getpixel(x, y);
    uint8_t bg_r = (bg_color >> 16) & 0xFF;
    uint8_t bg_g = (bg_color >> 8) & 0xFF;
    uint8_t bg_b = bg_color & 0xFF;
    uint8_t out_r = ((r * a) + (bg_r * (255 - a))) / 255;
    uint8_t out_g = ((g * a) + (bg_g * (255 - a))) / 255;
    uint8_t out_b = ((b * a) + (bg_b * (255 - a))) / 255;
    uint32_t final_color = (out_r << 16) | (out_g << 8) | out_b;
    fb_putpixel(x, y, final_color);
}

void gfx_draw_alpha_rect(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int x, int y) {
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            gfx_putpixel_alpha(x + xx, y + yy, r, g, b, a);
        }
    }
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    fb_draw_rect(x, y, w, h, color);
}

#define abs(x) ((x) < 0 ? -(x) : (x))

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        gfx_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// --- GÜNCELLENMİŞ METİN ÇİZİMİ ---
void gfx_draw_text(int x, int y, uint32_t color, const char* s) {    
    if (!s) return;
    
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        
        if (c >= 14 && c <= 25) {
            const uint8_t* glyph = tr_glyphs[c - 14]; 
            for (int row = 0; row < 16; row++) {
                uint8_t line = glyph[row];
                for (int col = 0; col < 8; col++) {
                    if (line & (1u << (7 - col))) {
                        // KRİTİK AYAR: y + row - 8 
                        // Harfi 8 piksel yukarıdan başlatıyoruz ki taban çizgisi eşitleşsin.
                        gfx_putpixel(x + col, y + row - 8, color); 
                    }
                }
            }
        } else {
            // Standart 8x8 font çizimi (Dokunma)
            const uint8_t* glyph = font8x8_basic[c];
            for (int row = 0; row < 8; row++) {
                uint8_t line = glyph[row];
                for (int col = 0; col < 8; col++) {
                    if (line & (1u << (7 - col))) {
                        gfx_putpixel(x + col, y + row, color);
                    }
                }
            }
        }
        x += 8; 
    }
}

void gfx_draw_text_debug(int x, int y, uint32_t text_color, const char* s) {
    if (!s) return;
    uint32_t debug_bg = 0x0000FF; // Parlak Mavi Arka Plan

    while (*s) {
        uint8_t c = (uint8_t)*s++;
        
        // 1. Önce 8x8'lik MAVİ kutuyu çiz (Hizalamayı görmek için)
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                gfx_putpixel(x + j, y + i, debug_bg);
            }
        }

        // 2. Harfi bu mavi kutunun üzerine çiz
        const uint8_t* glyph = font8x8_basic[c];
        for (int row = 0; row < 8; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (line & (1u << (7 - col))) {
                    gfx_putpixel(x + col, y + row, text_color);
                }
            }
        }
        
        // Harfler arasında 1 piksel boşluk bırakalım ki kutular birbirine yapışmasın
        x += 9; 
    }
}

static uint32_t isqrt_u32(uint32_t n) {
    uint32_t x = n;
    uint32_t y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }
    return x;
}

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