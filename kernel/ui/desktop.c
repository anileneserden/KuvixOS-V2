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
#include <kernel/drivers/input/keyboard.h>
#include <kernel/time.h>
#include <ui/apps/settings/settings_app.h>
#include "icons/terminal/terminal.h"
#include <app/app.h>
#include <app/app_manager.h>
#include <ui/ui_manager.h>

// ✅ Yeni include: input manager
#include <kernel/drivers/input/input_manager.h>

// --- Harici Tanımlar ---
extern ui_select_t sel_res;
extern void terminal_handle_key(char c);
extern void appmgr_init(void);
extern app_t* appmgr_start_app(int app_id);
extern int kbd_has_character(void);
extern app_t* appmgr_get_app_by_window_id(int win_id);
extern int g_current_mode;

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
    if (gx < 0) gx = 0;
    if (gy < 0) gy = 0;
    *x = GRID_OFFSET_X + (gx * GRID_SIZE_W);
    *y = GRID_OFFSET_Y + (gy * GRID_SIZE_H);
}

typedef struct { int x, y; const char* label; bool dragging; int app_id; } desktop_icon_t;
static desktop_icon_t terminal_icon = {40, 60, "Terminal", false, 1};
static desktop_icon_t file_manager_icon = {140, 60, "Dosyalar", false, 3};

static void draw_desktop_icon(desktop_icon_t* icon, int mx, int my) {
    if (mx >= icon->x && mx <= icon->x + 32 && my >= icon->y && my <= icon->y + 32) {
        gfx_fill_rect(icon->x - 4, icon->y - 4, 40, 48, 0x334455);
    }

    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 20; c++) {
            uint8_t p = terminal_icon_bitmap[r][c];
            if (p == 1) fb_putpixel(icon->x + c, icon->y + r, 0xFFFFFF);
            else if (p == 2) fb_putpixel(icon->x + c, icon->y + r, 0x222222);
            else if (p == 3) fb_putpixel(icon->x + c, icon->y + r, 0x00FF00);
        }
    }
    gfx_draw_text(icon->x - 5, icon->y + 24, 0xFFFFFF, icon->label);
}

static void ui_topbar_draw(void) {
    int sw = (int)fb_get_width();
    gfx_fill_rect(0, 0, sw, 24, 0x1A1A1A);
    gfx_draw_text(10, 6, 0xAAAAAA, "KuvixOS V2");
    char buf[16];
    local_itoa(g_ticks_ms / 1000, buf);
    gfx_draw_text(sw - 60, 6, 0x00FF00, buf);
}

static void draw_debug_info(void) {
    fb_draw_rect(5, 30, 150, 20, 0x000000);
    gfx_draw_text(10, 35, 0xFFFF00, "Odak:");

    int active = wm_get_active_id();
    if (active == -1) {
        gfx_draw_text(60, 35, 0x00FF00, "Masaustu");
    } else {
        char buf[8]; local_itoa(active, buf);
        gfx_draw_text(60, 35, 0xFFFFFF, buf);
    }
}

// ✅ Artık “run loop” değil: her çağrıda 1 frame
void ui_desktop_run(void) {
    static uint8_t last_btn = 0;
    static bool initialized = false;

    // 1) Sadece bir kez init
    if (!initialized) {
        wm_init();
        appmgr_init();
        settings_init();
        // ❌ ps2_mouse_init();  -> input_init() yapacak
        initialized = true;
    }

    // Eğer mod desktop değilse hiçbir şey yapma (güvenli)
    if (g_current_mode != MODE_DESKTOP) return;

    // 2) Klavye (event tüketme)
    if (kbd_has_character()) {
        uint16_t event = kbd_pop_event();
        uint8_t scancode = event & 0x7F;
        bool pressed = !(event & 0x80);

        if (pressed) {
            // F2 -> 3D
            if (scancode == 0x3C) {
                g_current_mode = MODE_3D_RENDER;
                return;
            }

            char c = 0;
            if (scancode >= 0x02 && scancode <= 0x0B) c = "1234567890"[scancode - 0x02];
            else if (scancode >= 0x10 && scancode <= 0x19) c = "qwertyuiop"[scancode - 0x10];
            else if (scancode == 0x1C) c = '\n';
            else if (scancode == 0x39) c = ' ';

            if (c != 0) {
                int active_win = wm_get_active_id();
                if (active_win != -1) {
                    app_t* active_app = appmgr_get_app_by_window_id(active_win);
                    if (active_app && active_app->v && active_app->v->on_key) {
                        active_app->v->on_key(active_app, (uint16_t)c);
                    }
                }
            }
        }
    }

    // 3) Mouse: input_manager absolute mouse_x/y güncelledi.
    // Buton state'i de input_manager'dan alacağız.
    uint8_t btn = input_mouse_buttons();

    wm_handle_mouse_move(mouse_x, mouse_y);
    wm_handle_mouse(mouse_x, mouse_y, (btn & ~last_btn), (~btn & last_btn), btn);
    last_btn = btn;

    // 4) Draw
    fb_clear(0x182838);

    draw_desktop_icon(&terminal_icon, mouse_x, mouse_y);
    // draw_desktop_icon(&file_manager_icon, mouse_x, mouse_y);

    wm_draw();
    ui_topbar_draw();
    draw_debug_info();
    cursor_draw_arrow(mouse_x, mouse_y);

    fb_present();
}
