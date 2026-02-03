#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <font/font8x16_tr.h>
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
    // Ekran sınırlarını kontrol etmeyi unutma
    if (x < 0 || y < 0 || (uint32_t)x >= fb_get_width() || (uint32_t)y >= fb_get_height()) return;
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

void gfx_draw_char(int x, int y, uint32_t color, unsigned char c) {
    // 1. Font dizisine erişim: Her karakter 16 byte (satır) uzunluğunda
    // font8x16_tr[c] bize o karakterin 16 satırlık verisini verir.
    
    for (int row = 0; row < 16; row++) {
        // Karakterin o satırdaki 8 bitlik verisini alıyoruz
        uint8_t row_data = font8x16_tr[c][row];

        for (int col = 0; col < 8; col++) {
            // Bit kontrolü: En anlamlı bitten (MSB) başlıyoruz
            // (1 << (7 - col)) mantığı bitleri soldan sağa okumamızı sağlar
            if (row_data & (1 << (7 - col))) {
                gfx_putpixel(x + col, y + row, color);
            }
        }
    }
}

// --- GÜNCELLENMİŞ METİN ÇİZİMİ ---
void gfx_draw_text(int x, int y, uint32_t color, const char* s) {    
    if (!s) return;
    
    // İşaretçiyi unsigned char yaparak 128 üstü (Türkçe) karakterleri güvenle oku
    unsigned char* ptr = (unsigned char*)s;
    
    while (*ptr) {
        gfx_draw_char(x, y, color, *ptr);
        x += 8; // Her harf 8 piksel genişliğinde (sabit genişlikli font)
        ptr++;
    }
}

// --- HİZALAMA VE KUTU GÖRSELLEŞTİRME TESTİ ---
void gfx_draw_text_debug(int x, int y, uint32_t text_color, const char* s) {
    if (!s) return;
    uint32_t debug_bg = 0xCC0000; // Kutuları kırmızı yapalım

    unsigned char* ptr = (unsigned char*)s;
    while (*ptr) {
        // 1. 8x16'lık KIRMIZI kutuyu çiz (Harfin kapladığı alanı gör)
        gfx_fill_rect(x, y, 8, 16, debug_bg);

        // 2. Harfi üzerine çiz
        gfx_draw_char(x, y, text_color, *ptr);
        
        x += 9; // Kutular arası 1 piksel boşluk
        ptr++;
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