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
    char buf[16];
    local_itoa(g_ticks_ms / 1000, buf);
    gfx_draw_text(sw - 60, 6, 0x00FF00, buf);
}

void draw_debug_info(void) {
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
        // 1. Donanımdan ham verileri oku (Kritik: Veri gelmezse kuyruk dolmaz)
        ps2_mouse_poll();

        // 2. KLAVYE KONTROLÜ
        if (kbd_has_character()) {
            char c = kbd_get_char();
            int active_win = wm_get_active_id();
            if (active_win != -1) {
                app_t* active_app = appmgr_get_app_by_window_id(active_win);
                if (active_app && active_app->v && active_app->v->on_key) {
                    active_app->v->on_key(active_app, (uint16_t)c); 
                }
            }
        }

        // 3. FARE KONTROLÜ VE ETKİLEŞİM
        while (ps2_mouse_pop(&dx, &dy, &btn)) {
            // Koordinat güncelleme
            mouse_x += dx; 
            mouse_y += dy;

            // Ekran sınırlarını koru
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > (int)fb_get_width() - 2) mouse_x = (int)fb_get_width() - 2;
            if (mouse_y > (int)fb_get_height() - 2) mouse_y = (int)fb_get_height() - 2;

            // Tıklama ve bırakma olaylarını hesapla
            uint8_t pressed = btn & ~last_btn;
            uint8_t released = ~btn & last_btn;

            // --- PENCERE YÖNETİMİ (WM) ---
            // Önce hareketi bildir (Pencere sürükleme burada gerçekleşir)
            wm_handle_mouse_move(mouse_x, mouse_y);
            // Sonra tıklama/bırakma olaylarını bildir
            wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);

            // --- İKON KONTROLÜ (Sadece boş masaüstündeyken) ---
            if (wm_find_window_at(mouse_x, mouse_y) == -1) { 
                if (pressed & 1) { 
                    // Terminal İkonu Tıklama
                    if (mouse_x >= terminal_icon.x && mouse_x <= terminal_icon.x + 32 &&
                        mouse_y >= terminal_icon.y && mouse_y <= terminal_icon.y + 32) {
                        terminal_icon.dragging = true;
                        appmgr_start_app(terminal_icon.app_id); 
                    }
                    // Dosya Yöneticisi İkonu Tıklama
                    else if (mouse_x >= file_manager_icon.x && mouse_x <= file_manager_icon.x + 32 &&
                            mouse_y >= file_manager_icon.y && mouse_y <= file_manager_icon.y + 32) {
                        file_manager_icon.dragging = true;
                        appmgr_start_app(file_manager_icon.app_id); 
                    }
                }
            }

            // İkon sürükleme ve bırakma mantığı
            if (!(btn & 1)) { // Sol tuş bırakıldıysa
                if (terminal_icon.dragging) snap_to_grid(&terminal_icon.x, &terminal_icon.y);
                if (file_manager_icon.dragging) snap_to_grid(&file_manager_icon.x, &file_manager_icon.y);
                terminal_icon.dragging = false;
                file_manager_icon.dragging = false;
            } else { // Sol tuş basılıyken hareket ettir
                if (terminal_icon.dragging) { terminal_icon.x = mouse_x - 16; terminal_icon.y = mouse_y - 16; }
                if (file_manager_icon.dragging) { file_manager_icon.x = mouse_x - 16; file_manager_icon.y = mouse_y - 16; }
            }

            last_btn = btn;
        }

        // 4. ÇİZİM AŞAMASI (Double Buffering)
        fb_clear(0x182838); 
        
        // Katman 1: Arkaplan ve İkonlar
        draw_desktop_icon(&terminal_icon, mouse_x, mouse_y);
        draw_desktop_icon(&file_manager_icon, mouse_x, mouse_y); 

        // Katman 2: Uygulama Pencereleri (İçerikleriyle birlikte)
        wm_draw(); 
        
        // Katman 3: Sabit UI ve Panel
        ui_topbar_draw(); 
        draw_debug_info(); 
        
        // Katman 4: En Üst (Fare İmleci)
        cursor_draw_arrow(mouse_x, mouse_y); 
        
        // Çizilen tüm arka plan buffer'ını ekrana bas
        fb_present();
    }
}