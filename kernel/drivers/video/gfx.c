#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <ui/font8x8_basic.h>

void gfx_init(void) {
    // Ekranı başlangıç için siyahla temizle
    fb_clear(0x000000); 
    // Temizlenen siyah ekranı donanıma gönder
    fb_present();       
}

// Tüm ekranı bir ARGB rengiyle temizler
void gfx_clear(uint32_t color) {
    fb_clear(color);
}

// Temel piksel çizimi
void gfx_putpixel(int x, int y, uint32_t color) {
    fb_putpixel(x, y, color);
}

// r, g, b: Yeni rengin bileşenleri
// a: Saydamlık (0-255 arası, 0 tam saydam, 255 tam mat)
void gfx_putpixel_alpha(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || y < 0 || (uint32_t)x >= fb_get_width() || (uint32_t)y >= fb_get_height()) return;

    // 1. Arka plandaki mevcut rengi oku
    fb_color_t bg_color = fb_getpixel(x, y);

    // 2. Arka plan bileşenlerini ayır (Sistemin ARGB/BGRA olduğuna göre kaydırmaları ayarla)
    uint8_t bg_r = (bg_color >> 16) & 0xFF;
    uint8_t bg_g = (bg_color >> 8) & 0xFF;
    uint8_t bg_b = bg_color & 0xFF;

    // 3. Harmanlama (Blending) işlemi
    uint8_t out_r = ((r * a) + (bg_r * (255 - a))) / 255;
    uint8_t out_g = ((g * a) + (bg_g * (255 - a))) / 255;
    uint8_t out_b = ((b * a) + (bg_b * (255 - a))) / 255;

    // 4. Yeni rengi birleştir ve yaz
    uint32_t final_color = (out_r << 16) | (out_g << 8) | out_b;
    fb_putpixel(x, y, final_color);
}

// İstediğin özel parametre sıralamasıyla Alpha Rect
void gfx_draw_alpha_rect(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int x, int y) {
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            // Mevcut pikselleri alpha ile harmanlayarak boyar
            gfx_putpixel_alpha(x + xx, y + yy, r, g, b, a);
        }
    }
}

// Kare/Dikdörtgen çizimi
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    fb_draw_rect(x, y, w, h, color);
}

#define abs(x) ((x) < 0 ? -(x) : (x))

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        gfx_putpixel(x0, y0, color); // veya sende adı neyse (put_pixel vb.)
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
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