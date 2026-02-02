#include <stdint.h>
#include <stdbool.h>
#include <lib/string.h>
#include <ui/desktop.h>
#include <ui/wm.h>
#include <ui/cursor.h>
#include <ui/theme.h>
#include <ui/select/select.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/input/keyboard.h> // kbd_get_key burada tanımlı
#include <kernel/time.h>
#include <ui/apps/settings/settings_app.h>
#include "icons/terminal/terminal.h"

// --- Harici Tanımlar ---
extern int mouse_x, mouse_y;
extern ui_select_t sel_res;
extern void terminal_handle_key(char c);

#define GRID_SIZE_W 80
#define GRID_SIZE_H 80
#define GRID_OFFSET_X 20
#define GRID_OFFSET_Y 40

// --- Yardımcı Fonksiyonlar ---
static void local_itoa(int n, char* s) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do { s[i++] = n % 10 + '0'; } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    for (int j = 0, k = i-1; j < k; j++, k--) {
        char temp = s[j]; s[j] = s[k]; s[k] = temp;
    }
}

static void snap_to_grid(int* x, int* y) {
    int gx = (*x - GRID_OFFSET_X) / GRID_SIZE_W;
    int gy = (*y - GRID_OFFSET_Y) / GRID_SIZE_H;

    // GCC Misleading Indentation uyarısı için bloklar ayrıldı
    if (gx < 0) {
        gx = 0;
    }
    if (gy < 0) {
        gy = 0;
    }

    *x = GRID_OFFSET_X + (gx * GRID_SIZE_W);
    *y = GRID_OFFSET_Y + (gy * GRID_SIZE_H);
}

void debug_draw_number(int x, int y, int num, uint32_t color) {
    char buf[16];
    local_itoa(num, buf);
    gfx_draw_text(x, y, color, buf);
}

void draw_grid_debug(void) {
    // uint32_t cast edilerek sign-compare hatası çözüldü
    for (uint32_t x = (uint32_t)GRID_OFFSET_X; x < fb_get_width(); x += GRID_SIZE_W) {
        for (uint32_t y = (uint32_t)GRID_OFFSET_Y; y < fb_get_height(); y += GRID_SIZE_H) {
            fb_putpixel(x, y, 0x333333);
        }
    }
}

typedef struct { int x, y; const char* label; bool dragging; } desktop_icon_t;
static desktop_icon_t terminal_icon = {40, 60, "Terminal", false};

void draw_desktop_icon(desktop_icon_t* icon, int mx, int my) {
    if (mx >= icon->x && mx <= icon->x + 20 && my >= icon->y && my <= icon->y + 20) {
        gfx_fill_rect(icon->x - 2, icon->y - 2, 24, 24, 0x444444);
    }
    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 20; c++) {
            uint8_t p = terminal_icon_bitmap[r][c];
            if (p == 1) fb_putpixel(icon->x + c, icon->y + r, 0xFFFFFF);
            if (p == 2) fb_putpixel(icon->x + c, icon->y + r, 0x333333);
            if (p == 3) fb_putpixel(icon->x + c, icon->y + r, 0x00FF00);
        }
    }
    gfx_draw_text(icon->x - 5, icon->y + 22, 0xFFFFFF, icon->label);
}

void ui_topbar_draw(void) {
    int sw = (int)fb_get_width();
    gfx_fill_rect(0, 0, sw, 24, 0x1A1A1A); 
    char buf[16];
    local_itoa(g_ticks_ms / 1000, buf);
    gfx_draw_text(sw - 60, 6, 0x00FF00, buf);
}

void draw_debug_info(void) {
    fb_draw_rect(5, 5, 180, 18, 0x000000); 
    gfx_draw_text(10, 10, 0xFFFF00, "X:"); debug_draw_number(30, 10, mouse_x, 0xFFFFFF);
    gfx_draw_text(70, 10, 0xFFFF00, "Y:"); debug_draw_number(90, 10, mouse_y, 0xFFFFFF);
}

// --- desktop.c içindeki Harici Tanımlar kısmına ekle ---
extern int kbd_has_character(void);
extern char kbd_get_char(void);
extern void* terminal_app_instance;
extern void terminal_handle_key(char c);

// --- ANA DÖNGÜ ---
void ui_desktop_run(void) {
    int dx, dy;
    uint8_t btn;
    uint8_t last_btn = 0;

    wm_init();
    settings_init();
    
    while(1) {
        // Klavye kontrolü - kbd_has_character ve kbd_get_char kullanıyoruz
        if (kbd_has_character()) {
            char c = kbd_get_char();
            // Eğer bir karakter döndüyse ve pencere aktifse gönder
            if (c != 0 && wm_get_active_id() != -1) {
                terminal_handle_key(c);
            }
        }

        // --- FARE KONTROLÜ ---
        while (ps2_mouse_pop(&dx, &dy, &btn)) {
            mouse_x += dx; mouse_y += dy;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > (int)fb_get_width() - 5) mouse_x = (int)fb_get_width() - 5;
            if (mouse_y > (int)fb_get_height() - 5) mouse_y = (int)fb_get_height() - 5;

            uint8_t pressed = btn & ~last_btn;
            uint8_t released = ~btn & last_btn;

            wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);

            if (wm_get_active_id() == -1) {
                if (pressed & 1) {
                    if (mouse_x >= terminal_icon.x && mouse_x <= terminal_icon.x + 20 &&
                        mouse_y >= terminal_icon.y && mouse_y <= terminal_icon.y + 20) {
                        
                        terminal_icon.dragging = true; 
                        // Pointer olarak gönderiyoruz
                        wm_add_window(150, 150, 400, 250, "Terminal", (app_t*)&terminal_app_instance);
                    }
                }
            }

            if (terminal_icon.dragging) {
                if (btn & 1) {
                    terminal_icon.x = mouse_x - 10;
                    terminal_icon.y = mouse_y - 10;
                } else {
                    snap_to_grid(&terminal_icon.x, &terminal_icon.y);
                    terminal_icon.dragging = false;
                }
            }
            last_btn = btn;
        }

        // --- ÇİZİM ---
        fb_clear(0x182838);
        draw_grid_debug();
        draw_desktop_icon(&terminal_icon, mouse_x, mouse_y);
        wm_draw();
        ui_topbar_draw();
        draw_debug_info();
        cursor_draw_arrow(mouse_x, mouse_y);
        fb_present();
    }
}