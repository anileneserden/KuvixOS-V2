#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>

// 1. Fonksiyon Ön Bildirimleri (Parametreler vtable ile tam uyumlu hale getirildi)
void terminal_on_create(app_t* app);
void terminal_on_draw(app_t* app);
void terminal_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons);
// uint32_t yerine uint16_t kullanıyoruz
void terminal_on_key(app_t* app, uint16_t key);

// Dosyanın başına buffer ekle
static char term_buffer[64] = {0};
static int term_ptr = 0;

// 2. VTABLE (Artık derleyici tiplerden şikayet etmeyecek)
const app_vtbl_t terminal_vtbl = {
    .on_create  = terminal_on_create,
    .on_draw    = terminal_on_draw,
    .on_mouse   = terminal_on_mouse, // Parametre hatası burada çözüldü
    .on_key     = terminal_on_key,
    .on_destroy = 0
};

// 3. Fonksiyon İçerikleri
void terminal_on_create(app_t* app) {
    (void)app;
}

void terminal_on_draw(app_t* app) {
    if (!app) return;
    ui_rect_t client = wm_get_client_rect(app->win_id);
    
    gfx_fill_rect(client.x, client.y, client.w, client.h, 0xFF000000);
    gfx_draw_text(client.x + 10, client.y + 10, 0xFF00FF00, "KuvixOS Terminal v1.0");
    
    // Ekrana statik "> _" yerine buffer'ı yaz
    char display[128] = "> ";
    strcat(display, term_buffer);
    strcat(display, "_");
    
    gfx_draw_text(client.x + 10, client.y + 30, 0xFF00FF00, display);
}

// Parametre listesi wm.h / app.h içindeki vtable tanımına uyarlandı
void terminal_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)app; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
}

void terminal_on_key(app_t* app, uint16_t key) {
    if (!app) return;

    // Enter tuşu (Genelde 10 veya 13'tür, sistemine göre kontrol et)
    if (key == '\n' || key == 13) {
        term_ptr = 0;
        term_buffer[0] = '\0';
    } 
    // Backspace (8)
    else if (key == 8 && term_ptr > 0) {
        term_buffer[--term_ptr] = '\0';
    }
    // Normal karakterler (ASCII kontrolü)
    else if (key >= 32 && key <= 126 && term_ptr < 63) {
        term_buffer[term_ptr++] = (char)key;
        term_buffer[term_ptr] = '\0';
    }
}