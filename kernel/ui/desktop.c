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
#include <kernel/drivers/input/keyboard.h>
#include <kernel/time.h>
#include <ui/apps/settings/settings_app.h>
#include "icons/terminal/terminal.h"
#include <app/app.h>
#include <app/app_manager.h>
#include <ui/window/window.h>
#include <lib/game_engine.h>

// --- Harici Tanımlar ---
extern int mouse_x, mouse_y;
extern ui_select_t sel_res;
extern void terminal_handle_key(char c);
extern void appmgr_init(void);
extern app_t* appmgr_start_app(int app_id);
extern int kbd_has_character(void);
extern char kbd_get_char(void);
extern app_t* appmgr_get_app_by_window_id(int win_id);

#define GRID_SIZE_W 80
#define GRID_SIZE_H 80
#define GRID_OFFSET_X 20
#define GRID_OFFSET_Y 40

// --- DEBUG İÇİN GLOBAL DEĞİŞKEN ---
static uint16_t last_raw_scancode = 0; // En son gelen ham kodu tutar

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
static desktop_icon_t game_engine_icon = {140, 60, "Game Engine", false, 1};
static desktop_icon_t terminal_icon = {40, 60, "Terminal", false, 2};
// static desktop_icon_t file_manager_icon = {140, 60, "Dosyalar", false, 3};

void draw_desktop_icon(desktop_icon_t* icon, int mx, int my) {
    // Hover efekti (İkonun üzerine gelince hafif aydınlanma)
    if (mx >= icon->x && mx <= icon->x + 32 && my >= icon->y && my <= icon->y + 32) {
        gfx_fill_rect(icon->x - 4, icon->y - 4, 40, 48, 0x334455);
    }
    
    // İkon Bitmap Çizimi (Burada terminal_icon_bitmap kullanılmış)
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

void ui_topbar_draw(void) {
    int sw = (int)fb_get_width();
    gfx_fill_rect(0, 0, sw, 24, 0x1A1A1A); 
    gfx_draw_text(10, 6, 0xAAAAAA, "KuvixOS V2");

    // --- BURAYI EKLE/GÜNCELLE ---
    // Ham Scancode Bilgisi
    char code_buf[32];
    local_itoa(last_raw_scancode, code_buf);
    gfx_draw_text(150, 6, 0xFFFF00, "Ham Kod:"); 
    gfx_draw_text(220, 6, 0xFFFFFF, code_buf);

    if (last_raw_scancode == 0x1E) {
        gfx_draw_text(260, 6, 0x00FF00, "[A TUSU OK]"); 
    } else if (last_raw_scancode == 0x20) {
        gfx_draw_text(260, 6, 0x00FF00, "[D TUSU OK]");
    }
    // ----------------------------

    char buf[16];
    local_itoa(g_ticks_ms / 1000, buf);
    gfx_draw_text(sw - 60, 6, 0x00FF00, buf);
}

void draw_debug_info(void) {
    // Bilgi kutusunu çiz
    fb_draw_rect(5, 30, 180, 45, 0x000000); 
    gfx_draw_text(10, 35, 0xFFFF00, "Odak:"); 
    
    int active = wm_get_active_id();
    if (active == -1) {
        gfx_draw_text(60, 35, 0x00FF00, "Masaustu");
    } else {
        // ID Yazdır
        char buf[16]; 
        local_itoa(active, buf);
        gfx_draw_text(60, 35, 0xFFFFFF, buf);

        // --- AKTİF PENCERE BOYUTUNU ÇEK ---
        ui_window_t win;
        // wm.c'deki wm_get_window fonksiyonunu kullanarak veriyi çekiyoruz
        if (wm_get_window(active, &win)) {
            gfx_draw_text(10, 50, 0x00FFFF, "Size:");
            
            // Genişlik (w)
            char w_buf[10]; 
            local_itoa(win.w, w_buf);
            gfx_draw_text(60, 50, 0xFFFFFF, w_buf);
            
            gfx_draw_text(95, 50, 0xFFFFFF, "x");
            
            // Yükseklik (h)
            char h_buf[10]; 
            local_itoa(win.h, h_buf);
            gfx_draw_text(110, 50, 0xFFFFFF, h_buf);
        }
    }
}

// --- ANA DÖNGÜ ---
void ui_desktop_run(void) {
    int dx, dy;
    uint8_t btn;
    uint8_t last_btn = 0;

    // Sistemleri başlat
    wm_init();
    appmgr_init();
    settings_init();
    
    while(1) {
        // --- 1. DONANIMI OKU (MOUSE & KEYBOARD) ---
        ps2_mouse_poll();

        // Klavye İşleme (Unity'deki Input Manager'ın arka planı gibi çalışır)
        while (kbd_has_character()) { 
            uint16_t raw_code = kbd_pop_event();
            if (raw_code != 0) {
                last_raw_scancode = raw_code; // Debug için sakla

                // 0x80 biti set edilmişse (örn: 0x9E) tuş BIRAKILMIŞTIR.
                // 0x80 biti yoksa (örn: 0x1E) tuş BASILMIŞTIR.
                bool is_break_code = (raw_code & 0x80) != 0;
                uint8_t state = is_break_code ? 0 : 1;
                
                // MOTORUN HARİTASINI GÜNCELLE
                // Not: engine_internal_set_key fonksiyonun raw_code içinden 
                // 0x80'i temizleyip (key & 0x7F) tabloyu güncellemeli.
                engine_internal_set_key(raw_code, state); 

                // --- EVENT BAZLI SİSTEM (Terminal gibi uygulamalar için) ---
                int active_win = wm_get_active_id();
                if (active_win != -1) {
                    app_t* active_app = appmgr_get_app_by_window_id(active_win);
                    if (active_app && active_app->v && active_app->v->on_key) {
                        // Uygulamaya sadece 'basılma' anında temiz kod gönder
                        if (!is_break_code) {
                            active_app->v->on_key(active_app, raw_code & 0x7F);
                        }
                    }
                }
            }
        }

        // --- 2. MOUSE ETKİLEŞİMİ ---
        while (ps2_mouse_pop(&dx, &dy, &btn)) {
            mouse_x += dx; mouse_y += dy;
            // Sınır korumaları...
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > (int)fb_get_width() - 5) mouse_x = (int)fb_get_width() - 5;
            if (mouse_y > (int)fb_get_height() - 5) mouse_y = (int)fb_get_height() - 2;

            uint8_t pressed = btn & ~last_btn;
            uint8_t released = ~btn & last_btn;

            wm_handle_mouse_move(mouse_x, mouse_y);
            wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);

            // İkon Tıklama Mantığı
            if (wm_find_window_at(mouse_x, mouse_y) == -1) {
                if (pressed & 1) {
                    if (mouse_x >= terminal_icon.x && mouse_x <= terminal_icon.x + 32 &&
                        mouse_y >= terminal_icon.y && mouse_y <= terminal_icon.y + 32) {
                        appmgr_start_app(terminal_icon.app_id);
                    }
                    else if (mouse_x >= game_engine_icon.x && mouse_x <= game_engine_icon.x + 32 &&
                            mouse_y >= game_engine_icon.y && mouse_y <= game_engine_icon.y + 32) {
                        appmgr_start_app(game_engine_icon.app_id);
                    }
                }
            }
            last_btn = btn;
        }

        // --- 3. ÇİZİM (DOUBLE BUFFERING) ---
        // Saniyede 60 kez burası döner (Render Loop)
        fb_clear(0x182838); 
        
        draw_desktop_icon(&game_engine_icon, mouse_x, mouse_y);
        draw_desktop_icon(&terminal_icon, mouse_x, mouse_y);

        // Uygulama Pencereleri ve İÇLERİNDEKİ OYUNLAR
        // wm_draw -> app->on_draw -> game_on_draw -> engine_loop
        wm_draw(); 
        
        ui_topbar_draw(); 
        cursor_draw_arrow(mouse_x, mouse_y); 
        
        fb_present(); // Arka planı ekrana yansıt
    }
}