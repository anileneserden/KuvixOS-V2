#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>

// Harfleri hizalamak için sabitler
#define BOX_SIZE 8
#define SPACING 4

void debug_font_on_create(app_t* app) {
    (void)app;
}

void debug_font_on_draw(app_t* app) {
    if (!app) return;
    ui_rect_t client = wm_get_client_rect(app->win_id);
    
    // Arka planı siyah yap
    gfx_fill_rect(client.x, client.y, client.w, client.h, 0xFF000000);

    int start_x = client.x + 10;
    int start_y = client.y + 30;
    int x = start_x;
    int y = start_y;

    // Test edilecek karakter seti (Sırasıyla: alfabetik + senin Türkçe kodların)
    // 14-19: ğ, ş, ı, ö, ç, ü | 20-25: Ğ, Ş, İ, Ö, Ç, Ü
    uint8_t test_chars[] = {
        'a', 'b', 'c', 18, 'd', 'e', 'f', 'g', 14, 'h', 16, 'i', 22, // ç, ğ, ı, İ kıyaslaması
        'o', 17, 's', 15, 'u', 19,                                   // ö, ş, ü
        0 // String sonu
    };

    gfx_draw_text(client.x + 10, client.y + 10, 0xFFFF00, "Font Debug: Mavi=Alan, Kirmizi=Baseline");

    for (int i = 0; test_chars[i] != 0; i++) {
        uint8_t c = test_chars[i];

        // 1. Mavi Kutuyu ve Kırmızı Baseline'ı Çiz
        for (int r = 0; r < BOX_SIZE; r++) {
            for (int col = 0; col < BOX_SIZE; col++) {
                // 7. satırı (index 6) kırmızı yap (Baseline)
                uint32_t bg_color = (r == 6) ? 0xFFFF0000 : 0xFF0000FF; 
                gfx_putpixel(x + col, y + r, bg_color);
            }
        }

        // 2. Karakteri Çiz (font8x8_basic içinden)
        extern const uint8_t font8x8_basic[256][8];
        const uint8_t* glyph = font8x8_basic[c];
        
        for (int r = 0; r < 8; r++) {
            uint8_t row_data = glyph[r];
            for (int col = 0; col < 8; col++) {
                if (row_data & (1u << (7 - col))) {
                    gfx_putpixel(x + col, y + r, 0xFFFFFFFF); // Beyaz harf
                }
            }
        }

        // Koordinatları güncelle
        x += (BOX_SIZE + SPACING);
        if (x > client.x + client.w - 20) {
            x = start_x;
            y += (BOX_SIZE + SPACING + 4);
        }
    }
}

// Fare ve klavye etkileşimi şimdilik boş
void debug_font_on_mouse(app_t* app, int mx, int my, uint8_t p, uint8_t r, uint8_t b) { 
    (void)app; (void)mx; (void)my; (void)p; (void)r; (void)b; 
}
void debug_font_on_key(app_t* app, uint16_t key) { 
    (void)app; (void)key; 
}

// --- VTABLE TANIMI ---
const app_vtbl_t debug_font_vtbl = {
    .on_create  = debug_font_on_create,
    .on_draw    = debug_font_on_draw,
    .on_mouse   = debug_font_on_mouse,
    .on_key     = debug_font_on_key,
    .on_destroy = 0
};