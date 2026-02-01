#include <stdint.h>
#include <stdbool.h>
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
#include <kernel/drivers/rtc/rtc.h>
#include <ui/theme.h>
#include <kernel/time.h>
#include <kernel/printk.h>
#include <ui/window.h>
#include <ui/window_chrome.h>
#include <ui/wm/hittest.h>
#include <kernel/memory/kmalloc.h>

// İkon verisi (Projenizdeki yola göre kontrol edin)
#include "icons/terminal/terminal.h"

extern app_t* terminal_app_create(void);
extern app_t* demo_app_create(void);

// Başka dosyalarda tanımlı global değişkenler
extern int mouse_x;
extern int mouse_y;

// --- Pencere Durum Yönetimi ---
static bool terminal_open = true;
static ui_window_t terminal_win = {
    .x = 150, .y = 100, 
    .w = 400, .h = 250, 
    .title = "Kuvix Terminal", 
    .state = WIN_NORMAL
};

// --- Masaüstü Çizim Yardımcıları ---
void desktop_draw_background(void) {
    const ui_theme_t* th = ui_get_theme();
    fb_clear(th->desktop_bg);
}

void draw_desktop_icon(int x, int y, const uint8_t bitmap[20][20], const char* label, int mx, int my) {
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

void ui_overlay_draw(void) {
    const ui_theme_t* th = ui_get_theme();
    int sw = fb_get_width();
    int sh = fb_get_height();
    int dock_h = th->dock_height;
    int dock_w = 420;
    int dock_x = (sw - dock_w) / 2;
    int dock_y = sh - dock_h - th->dock_margin_bottom;
    gfx_fill_round_rect(dock_x, dock_y, dock_w, dock_h, th->dock_radius, th->dock_bg);
}

void ui_topbar_draw(void) {
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
    uint8_t buttons, last_buttons = 0;
    
    // Etkileşim Değişkenleri
    bool is_dragging = false;
    bool is_resizing = false;
    wm_hittest_t active_resize_edge = HT_NONE;
    int drag_off_x = 0, drag_off_y = 0;

    while(1) {
        ps2_mouse_poll();

        // Hit-test: Fare neyin üzerinde?
        wm_hittest_t hit = HT_NONE;
        if (terminal_open) {
            hit = ui_chrome_hittest(&terminal_win, mouse_x, mouse_y);
        }

        while (ps2_mouse_pop(&dx, &dy, &buttons)) {
            mouse_x += dx; mouse_y += dy;

            // Ekran sınırları
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int)fb_get_width()) mouse_x = fb_get_width() - 1;
            if (mouse_y >= (int)fb_get_height()) mouse_y = fb_get_height() - 1;

            bool left_down = (buttons & 0x01);
            bool left_pressed = left_down && !(last_buttons & 0x01);

            if (terminal_open) {
                if (left_pressed) {
                    if (hit == HT_BTN_CLOSE) {
                        terminal_open = false;
                    } 
                    else if (hit >= HT_RESIZE_LEFT && hit <= HT_RESIZE_BOTTOM_RIGHT) {
                        is_resizing = true;
                        active_resize_edge = hit;
                    }
                    else if (hit == HT_TITLE) {
                        is_dragging = true;
                        drag_off_x = mouse_x - terminal_win.x;
                        drag_off_y = mouse_y - terminal_win.y;
                    }
                }

                // Taşıma Mantığı
                if (left_down && is_dragging) {
                    terminal_win.x = mouse_x - drag_off_x;
                    terminal_win.y = mouse_y - drag_off_y;
                    if (terminal_win.y < 24) terminal_win.y = 24;
                }

                // Boyutlandırma Mantığı (Resize)
                if (left_down && is_resizing) {
                    if (active_resize_edge == HT_RESIZE_RIGHT || active_resize_edge == HT_RESIZE_BOTTOM_RIGHT) {
                        terminal_win.w = mouse_x - terminal_win.x;
                    }
                    if (active_resize_edge == HT_RESIZE_BOTTOM || active_resize_edge == HT_RESIZE_BOTTOM_RIGHT) {
                        terminal_win.h = mouse_y - terminal_win.y;
                    }
                    if (terminal_win.w < 150) terminal_win.w = 150;
                    if (terminal_win.h < 100) terminal_win.h = 100;
                }
            } else {
                if (left_pressed && mouse_x >= 50 && mouse_x <= 70 && mouse_y >= 50 && mouse_y <= 70) {
                    terminal_open = true;
                }
            }

            if (!left_down) {
                is_dragging = false;
                is_resizing = false;
                active_resize_edge = HT_NONE;
            }
            last_buttons = buttons;
        }

        // --- Render Katmanları ---
        desktop_draw_background();
        draw_desktop_icon(50, 50, terminal_icon_bitmap, "Terminal", mouse_x, mouse_y);
        
        if (terminal_open) {
            ui_window_draw(&terminal_win, 1, mouse_x, mouse_y);
            ui_rect_t rect = ui_window_client_rect(&terminal_win);
            gfx_draw_text(rect.x + 5, rect.y + 5, 0x00FF00, "root@kuvixos # _");
        }

        ui_overlay_draw();
        ui_topbar_draw();

        // --- İmleç Çizimi ---
        wm_hittest_t current_hit = (is_resizing) ? active_resize_edge : hit;
        if (current_hit == HT_RESIZE_LEFT || current_hit == HT_RESIZE_RIGHT) {
            cursor_draw_resize_we(mouse_x, mouse_y);
        } else if (current_hit == HT_RESIZE_TOP || current_hit == HT_RESIZE_BOTTOM) {
            cursor_draw_resize_ns(mouse_x, mouse_y);
        } else {
            cursor_draw_arrow(mouse_x, mouse_y);
        }

        fb_present();
        for(volatile int i=0; i<5000; i++);
    }
}

// Linker Köprüleri
void input_init(void) { kbd_init(); }
void input_poll(void) { }
uint16_t input_kbd_pop_event(void) { return kbd_pop_event(); }
int input_mouse_pop(int* dx, int* dy, uint8_t* b) { return ps2_mouse_pop(dx, dy, b); }