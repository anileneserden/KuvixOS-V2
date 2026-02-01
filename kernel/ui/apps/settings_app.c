#include <ui/apps/settings/settings_app.h>
#include <ui/window/window.h>
#include <ui/ui_button/ui_button.h>
#include <ui/select/select.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h> // strcmp için

ui_window_t settings_win = {
    .x = 200, .y = 200, .w = 300, .h = 200,
    .title = "Sistem Ayarlari", .state = WIN_NORMAL
};

ui_button_t btn_apply;
ui_select_t sel_res;
static const char* options[] = {"800x600", "1280x720", "1920x1080"};

// settings_app.c içinde
void settings_init(void) {
    // Önce pencereyi oluştur veya konumunu belirle
    // settings_win.x = 200; settings_win.y = 150; vb.

    int padding_x = 20;
    int padding_y = 40; // Başlık çubuğunun altından başlaması için

    // Select kutusunu pencerenin içine yerleştir
    ui_select_init(&sel_res, 
                   settings_win.x + padding_x, 
                   settings_win.y + padding_y, 
                   150, 25, 
                   options, 3, 0);

    // Butonu select kutusunun altına yerleştir
    ui_button_init(&btn_apply, 
                   settings_win.x + padding_x, 
                   settings_win.y + padding_y + 50, 
                   80, 25, 
                   "Uygula");
}

void settings_update(int mx, int my, uint8_t b) {
    // Bileşenleri pencereye sabitle
    sel_res.x = settings_win.x + 20;
    sel_res.y = settings_win.y + 40;
    btn_apply.x = settings_win.x + 20;
    btn_apply.y = settings_win.y + 80;

    // UI bileşenlerini güncelle
    ui_select_update(&sel_res, mx, my);
    ui_select_on_mouse(&sel_res, mx, my, (b & 1), 0, b);

    if (ui_button_update(&btn_apply)) {
        // HATA BURADAYDI: selected_index yerine selected kullanıyoruz
        int idx = sel_res.selected; 

        if (idx == 0) {
            fb_set_resolution(1920, 1080);
        } else if (idx == 1) {
            fb_set_resolution(1280, 720);
        } else if (idx == 2) {
            fb_set_resolution(800, 600);
        }
    }
}

// settings_app.c içindeki settings_draw fonksiyonu
void settings_draw(int mx, int my) {
    // settings_win zaten global veya extern olarak tanımlı olmalı
    ui_window_draw(&settings_win, mx, my, 0); // Şimdilik aktiflik 0 (veya mantığa göre 1)
    
    ui_select_draw(&sel_res);
    ui_button_draw(&btn_apply);
}