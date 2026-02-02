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
    // WM'den pencerenin o anki gerçek koordinatlarını al
    ui_rect_t client = wm_get_client_rect(app->win_id);
    
    // Arka planı pencerenin YENİ konumuna göre çiz
    gfx_fill_rect(client.x, client.y, client.w, client.h, 0xFF000000);
    
    // Yazıları pencerenin içine (client.x ve client.y ofsetli) yaz
    gfx_draw_text(client.x + 10, client.y + 10, 0xFF00FF00, "KuvixOS Terminal v1.0");
    gfx_draw_text(client.x + 10, client.y + 30, 0xFF00FF00, "> _");
}

// Parametre listesi wm.h / app.h içindeki vtable tanımına uyarlandı
void terminal_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)app; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
}

void terminal_on_key(app_t* app, uint16_t key) {
    (void)app; (void)key;
}