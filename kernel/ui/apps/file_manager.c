#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/theme.h>
#include <app/app.h>
#include <kernel/drivers/video/fb.h>

// Dosya yöneticisi için tasarım değerleri
#define COLOR_SIDEBAR 0xD0D0D0
#define COLOR_LIST_BG 0xFFFFFF
#define SIDEBAR_WIDTH 120

// 1. Fonksiyonun parametresini (app_t* a) olarak güncelledik
void file_manager_on_draw(app_t* a) {
    ui_rect_t r = wm_get_client_rect(a->win_id);
    
    // KuvixOS'ta standart başlık çubuğu yüksekliği genelde 24-25'tir.
    // Başlık çubuğunu ezmemek için 'y' koordinatını 24 piksel aşağıdan başlatıyoruz.
    int content_y = r.y + 24; 
    int content_h = r.h - 24;

    // 1. Sağ taraf: Ana Liste Alanı (Beyaz)
    fb_draw_rect(r.x + SIDEBAR_WIDTH, content_y, r.w - SIDEBAR_WIDTH, content_h, COLOR_LIST_BG);

    // 2. Sol taraf: Sidebar (Gri)
    fb_draw_rect(r.x, content_y, SIDEBAR_WIDTH, content_h, COLOR_SIDEBAR);
    
    // Ayırıcı çizgi
    fb_draw_rect(r.x + SIDEBAR_WIDTH - 1, content_y, 1, content_h, 0xAAAAAA);

    // Metinleri de yeni y (content_y) değerine göre çiz
    gfx_draw_text(r.x + 10, content_y + 10, 0x333333, "Dizinler");
    gfx_draw_text(r.x + 15, content_y + 35, 0x0000FF, "- Kok (/)");
    gfx_draw_text(r.x + SIDEBAR_WIDTH + 20, content_y + 10, 0x000000, "Dosya Adi");
}

// Boş bir mouse fonksiyonu ekle
void file_manager_on_mouse(app_t* a, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)a; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
    // Burası boş olsa bile vtable'da tanımlı olması WM'e "bu interaktif bir uygulama" sinyali verir.
}

// vtable kısmını güncelle
app_vtbl_t file_manager_app = {
    .on_create   = 0,
    .on_destroy  = 0,
    .on_mouse    = file_manager_on_mouse, // Artık 0 değil!
    .on_key      = 0,
    .on_update   = 0,
    .on_draw     = file_manager_on_draw 
};