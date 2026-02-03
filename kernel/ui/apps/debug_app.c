#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <font/font8x16_tr.h> // Yeni font header'ı
#include <lib/string.h>

// 8x16 font için yeni hizalama sabitleri
#define BOX_W 8
#define BOX_H 16
#define SPACING 6

void debug_font_on_create(app_t* app) {
    (void)app;
}

void debug_font_on_draw(app_t* app) {
    if (!app) return;
    ui_rect_t client = wm_get_client_rect(app->win_id);
    
    // Arka planı koyu lacivert yap (Harfler beyaz daha iyi görünür)
    gfx_fill_rect(client.x, client.y, client.w, client.h, 0xFF1A1A2E);

    int start_x = client.x + 10;
    int start_y = client.y + 40;
    int x = start_x;
    int y = start_y;

    // Test edilecek karakter seti (Latin-5 / ISO-8859-9 kodları)
    // Sırasıyla: Büyük ve Küçük Türkçe Karakterler kıyaslaması
    uint8_t test_chars[] = {
        'A', 'B', 'C', 0xC7, 'G', 0xD0, 'I', 0xDD, 'O', 0xD6, 'S', 0xDE, 'U', 0xDC, // Büyükler
        '\n', // Alt satıra geçmek için işaretçi (aşağıda işleyeceğiz)
        'a', 'b', 'c', 0xE7, 'g', 0xF0, 0xFD, 'i', 'o', 0xF6, 's', 0xFE, 'u', 0xFC, // Küçükler
        0 // Bitiş
    };

    gfx_draw_text(client.x + 10, client.y + 10, 0xFFFF00, "Font 8x16 Debug: Mavi=Box, Kirmizi=Baseline");

    for (int i = 0; test_chars[i] != 0; i++) {
        uint8_t c = test_chars[i];

        if (c == '\n') {
            x = start_x;
            y += (BOX_H + 15);
            continue;
        }

        // 1. Kutu ve Baseline Çizimi (Karakterin sınırlarını görmek için)
        for (int r = 0; r < BOX_H; r++) {
            for (int col = 0; col < BOX_W; col++) {
                // 13. satırı kırmızı yap (Alt baseline / kuyrukların başladığı yer)
                uint32_t bg_color = (r == 12) ? 0xFFFF0000 : 0xFF000088; 
                gfx_putpixel(x + col, y + r, bg_color);
            }
        }

        // 2. Karakteri Çiz
        // Artık merkezi gfx_draw_char fonksiyonunu kullanıyoruz (16 satır destekli)
        gfx_draw_char(x, y, 0xFFFFFFFF, c);

        // Koordinatları güncelle
        x += (BOX_W + SPACING);
        
        // Ekran sonu kontrolü
        if (x > client.x + client.w - 20) {
            x = start_x;
            y += (BOX_H + 15);
        }
    }
}

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