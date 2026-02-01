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

static void format_date_simple(char* out, uint8_t day, uint8_t mon, uint16_t year) {
    // DD/MM/YYYY formatı (Örn: 01/02/2026)
    out[0] = (char)('0' + (day / 10));
    out[1] = (char)('0' + (day % 10));
    out[2] = '/';
    out[3] = (char)('0' + (mon / 10));
    out[4] = (char)('0' + (mon % 10));
    out[5] = '/';
    // Yılın binler, yüzler, onlar ve birler basamağı
    out[6] = (char)('0' + (year / 1000));
    out[7] = (char)('0' + ((year / 100) % 10));
    out[8] = (char)('0' + ((year / 10) % 10));
    out[9] = (char)('0' + (year % 10));
    out[10] = 0; // String sonu
}

// Saat formatı için yardımcı fonksiyon (desktop.c içine veya üstüne ekle)
void get_live_time_string(char* buffer) {
    // Toplam saniyeyi al
    uint32_t total_seconds = g_ticks_ms / 1000;
    
    // Doğru MOD hesaplamaları:
    uint32_t s = total_seconds % 60;          // 0-59 arası saniye
    uint32_t m = (total_seconds / 60) % 60;   // 0-59 arası dakika
    uint32_t h = (total_seconds / 3600) % 24; // 0-23 arası saat

    // Manuel formatlama (HH:MM:SS)
    buffer[0] = (h / 10) + '0';
    buffer[1] = (h % 10) + '0';
    buffer[2] = ':';
    buffer[3] = (m / 10) + '0';
    buffer[4] = (m % 10) + '0';
    buffer[5] = ':';
    buffer[6] = (s / 10) + '0';
    buffer[7] = (s % 10) + '0';
    buffer[8] = '\0';
}

extern volatile int test_counter; // time.c'deki değişkeni buraya tanıtıyoruz

// Uptime formatlamak için (Örn: 01:23:45)
static void format_time_hms(char* out, uint32_t h, uint32_t m, uint32_t s) {
    out[0] = (h / 10) + '0';
    out[1] = (h % 10) + '0';
    out[2] = ':';
    out[3] = (m / 10) + '0';
    out[4] = (m % 10) + '0';
    out[5] = ':';
    out[6] = (s / 10) + '0';
    out[7] = (s % 10) + '0';
    out[8] = 0;
}

void ui_topbar_draw(void) {
    int sw = fb_get_width();
    gfx_fill_rect(0, 0, sw, 24, 0x1A1A1A); // Barı çiz

    // --- HATA BURADAYDI ---
    // g_ticks_ms zaten milisaniye. Sadece 1000'e bölmek saniyeyi verir.
    // Eğer bir de 60'a bölersen (g_ticks_ms / 60000) dakikayı elde edersin.
    uint32_t total_seconds = g_ticks_ms / 1000; 

    char buf[16];
    int i = 0;
    uint32_t n = total_seconds; // Sadece saniyeyi basıyoruz

    if (n == 0) {
        buf[i++] = '0';
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (n > 0) {
            temp[j++] = (n % 10) + '0';
            n /= 10;
        }
        // Tek basamaklıysa başına 0 ekle (Örn: 05)
        if (j == 1) buf[i++] = '0'; 
        while (j > 0) buf[i++] = temp[--j];
    }
    buf[i] = '\0';

    // Sağ üst köşede yeşil saniye
    gfx_draw_text(sw - 50, 6, 0x00FF00, buf); 
}

/*
void ui_topbar_draw(void) {
    int sw = fb_get_width();
    int bar_h = 24;

    // 1. Üst Bar Arka Planı
    gfx_fill_rect(0, 0, sw, bar_h, 0x1A1A1A); 

    // 2. Canlı Saat (HH:MM:SS) - Senin hata aldığın kısım burasıydı
    char live_time[10];
    get_live_time_string(live_time);
    
    // Tek bir time_x değişkeni kullanıyoruz
    int t_pos_x = sw - 80; 
    gfx_draw_text(t_pos_x, 6, 0x00FF00, live_time); // Dikkat çekmesi için yeşil yaptık

    // 3. Hover Kontrolü (Mouse ile üzerine gelince açılan panel)
    if (mouse_x > sw - 80 && mouse_x < sw && mouse_y >= 0 && mouse_y < 24) {
        int panel_w = 160;
        int panel_h = 110;
        int panel_x = sw - panel_w - 5;
        int panel_y = 24 + 5;

        // Arka Plan
        gfx_fill_round_rect(panel_x, panel_y, panel_w, panel_h, 8, 0x222222);

        // RTC'den gelen tarih
        rtc_datetime_t now = time_now_datetime();
        char date_str[12];
        format_date_simple(date_str, now.day, now.month, now.year);
        
        gfx_draw_text(panel_x + 15, panel_y + 15, 0x00AAFF, "Sistem Saati");
        gfx_draw_text(panel_x + 15, panel_y + 35, 0xFFFFFF, date_str);

        // Uptime (Saniyeler canlı artacak)
        uint32_t total_s = g_ticks_ms / 1000;
        uint32_t up_h = total_s / 3600;
        uint32_t up_m = (total_s % 3600) / 60;
        uint32_t up_s = total_s % 60;

        char up_str[15];
        format_uptime_simple(up_str, up_h, up_m, up_s);
        
        gfx_draw_text(panel_x + 15, panel_y + 65, 0x00FF00, "Calisma Suresi");
        gfx_draw_text(panel_x + 15, panel_y + 85, 0xFFFFFF, up_str);
    }
}*/

void debug_draw_number(int x, int y, int num, uint32_t color) {
    char buf[16];
    int i = 0;

    // Negatif sayı kontrolü
    if (num < 0) {
        gfx_draw_text(x, y, color, "-");
        x += 8; // Eksi işareti için kaydır
        num = -num;
    }

    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0 && i < 15) {
            buf[i++] = (num % 10) + '0';
            num /= 10;
        }
    }
    buf[i] = '\0';

    // Sayı basamaklarını ters çevirip yazdır (çünkü mod alırken sondan başladık)
    char reversed[16];
    int j;
    for (j = 0; j < i; j++) {
        reversed[j] = buf[i - 1 - j];
    }
    reversed[j] = '\0';

    gfx_draw_text(x, y, color, reversed);
}

// --- Ana Masaüstü Döngüsü ---
void ui_desktop_run(void) {
    int dx, dy;
    uint8_t btn;

    while(1) {
        // 1. Mouse kuyruğunu boşalt ve koordinatları güncelle
        while (ps2_mouse_pop(&dx, &dy, &btn)) {
            mouse_x += dx;
            mouse_y += dy;

            // Ekran sınırlarını koru (Örn: 1024x768 ise)
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > 1020) mouse_x = 1020;
            if (mouse_y > 760) mouse_y = 760;
        }

        // 2. Arka planı çiz (Her karede temizlemek için)
        desktop_draw_background(); 

        // 3. DEBUG YAZILARI
        gfx_draw_text(10, 30, 0x00FF00, "MOUSE DEBUG:"); // Yeşil başlık
        
        gfx_draw_text(10, 50, 0xFFFF00, "X: ");
        debug_draw_number(35, 50, mouse_x, 0xFFFF00); // Sarı X
        
        gfx_draw_text(10, 70, 0xFFFF00, "Y: ");
        debug_draw_number(35, 70, mouse_y, 0xFFFF00); // Sarı Y

        gfx_draw_text(10, 90, 0xFF00FF, "BTN: ");
        debug_draw_number(50, 90, (int)btn, 0xFF00FF); // Pembe Buton durumu

        // 4. İMLEÇ (Kırmızı kare)
        gfx_fill_rect(mouse_x, mouse_y, 8, 8, 0xFF0000);

        fb_present();
    }
}

// Linker Köprüleri
void input_init(void) { kbd_init(); }
void input_poll(void) { }
uint16_t input_kbd_pop_event(void) { return kbd_pop_event(); }
int input_mouse_pop(int* dx, int* dy, uint8_t* b) { return ps2_mouse_pop(dx, dy, b); }