#include <stdint.h>
#include <ui/desktop.h>
#include <ui/wm.h>
#include <ui/mouse.h>
#include <ui/cursor.h>
#include <app/app.h>
#include <app/app_manager.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/input.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <font/font8x8_basic.h>
#include <ui/theme.h>
#include <arch/x86/io.h>
#include <ui/wallpaper.h>
#include <kernel/time.h>
#include <stdbool.h>
#include <kernel/printk.h>

// Yeni eklediğimiz pencere sistemleri
#include <ui/window.h>
#include <ui/window_chrome.h>
#include <ui/wm/hittest.h>

// İkon verisi
#include "icons/terminal/terminal.h"

// Fare koordinatları
extern int mouse_x;
extern int mouse_y;

// --- Pencere Durum Yönetimi ---
static bool terminal_open = false;
static ui_window_t terminal_win = {
    .x = 150, .y = 100, 
    .w = 400, .h = 250, 
    .title = "Kuvix Terminal", 
    .state = WIN_NORMAL
};

// --- Masaüstü Çizim Yardımcıları ---
static void desktop_draw_background(void) {
    const ui_theme_t* th = ui_get_theme();
    fb_clear(th->desktop_bg);
}

void demo_app_create(void) { 
    // Şimdilik boş
}

void terminal_app_create(void) { 
    // Şimdilik boş
}

static void draw_desktop_icon(int x, int y, const uint8_t bitmap[20][20], const char* label, int mx, int my) {
    if (mx >= x && mx <= x + 20 && my >= y && my <= y + 20) {
        gfx_fill_rect(x - 2, y - 2, 24, 24, 0x444444);
    }

    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 20; c++) {
            uint8_t p = bitmap[r][c];
            if (p == 1) fb_putpixel(x + c, y + r, 0xFFFFFF);
            if (p == 2) fb_putpixel(x + c, y + r, 0x333333);
            if (p == 3) fb_putpixel(x + c, y + r, 0x00FF00);
        }
    }
    gfx_draw_text(x - 5, y + 22, 0xFFFFFF, label);
}

static void ui_overlay_draw(void) {
    const ui_theme_t* th = ui_get_theme();
    int sw = fb_get_width();
    int sh = fb_get_height();
    int dock_h = th->dock_height;
    int dock_w = 420;
    int dock_x = (sw - dock_w) / 2;
    int dock_y = sh - dock_h - th->dock_margin_bottom;
    gfx_fill_round_rect(dock_x, dock_y, dock_w, dock_h, th->dock_radius, th->dock_bg);
}

static void ui_topbar_draw(void) {
    int sw = fb_get_width();
    int bar_h = 24;
    gfx_fill_rect(0, 0, sw, bar_h, 0x1A1A1A); 
    gfx_fill_rect(0, bar_h - 1, sw, 1, 0x333333);

    char time_str[9];
    time_format_hhmmss(time_str);
    gfx_draw_text(sw - 74, (bar_h - 8) / 2, 0xFFFFFF, time_str);
    gfx_draw_text(10, (bar_h - 8) / 2, 0x00AAFF, "KuvixOS");
}

// --- Ana Masaüstü Döngüsü ---
void ui_desktop_run(void) {
    input_init();
    time_init_from_rtc();
    ps2_mouse_init();

    int dx, dy;
    uint8_t buttons;
    uint8_t last_buttons = 0;
    bool menu_visible = false;
    int menu_x = 0, menu_y = 0;

    // --- Bu değişkenleri ui_desktop_run fonksiyonunun içinde, while(1)'in hemen üzerinde tanımla ---
    bool is_dragging = false;
    int drag_offset_x = 0;
    int drag_offset_y = 0;

    while(1) {
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            if (status & 0x20) { 
                ps2_mouse_handle_byte(inb(0x60));
            } else {
                volatile uint8_t kbd_data = inb(0x60); (void)kbd_data;
            }
        }

        while (ps2_mouse_pop(&dx, &dy, &buttons)) {
            mouse_x += dx;
            mouse_y += dy;

            // Ekran sınırları
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int)fb_get_width())  mouse_x = fb_get_width() - 1;
            if (mouse_y >= (int)fb_get_height()) mouse_y = fb_get_height() - 1;

            // Buton durumları
            bool left_down = (buttons & 0x01);
            bool left_pressed = (buttons & 0x01) && !(last_buttons & 0x01);

            // --- Etkileşim Kontrolleri ---
            
            if (left_pressed) {
                menu_visible = false; // Herhangi bir yere sol tıklandığında menüyü kapat

                // 1. İkon Tıklama Kontrolü (Sadece sürükleme yapmıyorsak)
                if (mouse_x >= 50 && mouse_x <= 70 && mouse_y >= 50 && mouse_y <= 70) {
                    terminal_open = true;
                    printk("Terminal acildi.\n");
                }

                // 2. Pencere Kontrolleri
                if (terminal_open) {
                    wm_hittest_t hit = ui_chrome_hittest(&terminal_win, mouse_x, mouse_y);
                    
                    if (hit == HT_BTN_CLOSE) {
                        terminal_open = false;
                        is_dragging = false;
                    } 
                    else if (hit == HT_TITLE) {
                        // Sürüklemeyi başlat
                        is_dragging = true;
                        drag_offset_x = mouse_x - terminal_win.x;
                        drag_offset_y = mouse_y - terminal_win.y;
                    }
                }
            }

            // 3. Sürükleme Devam Ediyor mu?
            if (left_down && is_dragging && terminal_open) {
                terminal_win.x = mouse_x - drag_offset_x;
                terminal_win.y = mouse_y - drag_offset_y;

                // --- ESNEK SINIR KONTROLÜ (%20 DIŞARI ÇIKABİLİR) ---
                int sw = (int)fb_get_width();
                int sh = (int)fb_get_height();
                int margin_w = terminal_win.w * 0.20; // Genişliğin %20'si
                int margin_h = terminal_win.h * 0.20; // Yüksekliğin %20'si

                // Sol Sınır: Pencerenin %80'i içeride kalmalı (x en fazla -margin_w olabilir)
                if (terminal_win.x < -margin_w) terminal_win.x = -margin_w;

                // Üst Sınır: Başlık çubuğu tamamen kaybolmasın diye TopBar (24px) altına sabitleyelim
                // Eğer başlığın da çıkmasını istersen -margin_h yapabilirsin
                if (terminal_win.y < 24) terminal_win.y = 24;

                // Sağ Sınır: Pencerenin sol tarafı ekranın sağından (sw - (win.w * 0.80)) fazla gidemez
                int max_x = sw - (terminal_win.w - margin_w);
                if (terminal_win.x > max_x) terminal_win.x = max_x;

                // Alt Sınır: Alt barın (dock) üzerine çok binmesin
                int max_y = sh - 40; 
                if (terminal_win.y > max_y) terminal_win.y = max_y;
            }

            // 4. Sürüklemeyi Bitir
            if (!left_down) {
                is_dragging = false;
            }

            // Sağ Tık Menüsü Açma
            if ((last_buttons & 0x02) && !(buttons & 0x02)) {
                menu_visible = true;
                menu_x = mouse_x; menu_y = mouse_y;
            }

            last_buttons = buttons;
        }

        // --- Çizim Katmanları (Z-Order) ---
        desktop_draw_background();                                         
        draw_desktop_icon(50, 50, terminal_icon_bitmap, "Term", mouse_x, mouse_y); 

        if (terminal_open) {                                               
            // Aktif pencere olduğu için 1 gönderiyoruz
            ui_window_draw(&terminal_win, 1, mouse_x, mouse_y);
            ui_rect_t rect = ui_window_client_rect(&terminal_win);
            gfx_draw_text(rect.x + 5, rect.y + 5, 0x00FF00, "root@kuvixos # _");
        }

        ui_overlay_draw();                                                 
        ui_topbar_draw();                                                  

        if (menu_visible) {                                                
            gfx_fill_rect(menu_x, menu_y, 130, 60, 0xFFFFFF);
            gfx_fill_rect(menu_x + 1, menu_y + 1, 128, 58, 0x222222);
            gfx_draw_text(menu_x + 10, menu_y + 10, 0xFFFFFF, "Yenile");
            gfx_draw_text(menu_x + 10, menu_y + 35, 0xFFFFFF, "Kapat");
        }

        cursor_draw_arrow(mouse_x, mouse_y);                               
        fb_present();
        
        for(volatile int i=0; i<5000; i++);
    }
}

// Linker Köprüleri
void input_init(void) { kbd_init(); }
void input_poll(void) { }
uint16_t input_kbd_pop_event(void) { return kbd_pop_event(); }
int input_mouse_pop(int* dx, int* dy, uint8_t* b) { return ps2_mouse_pop(dx, dy, b); }