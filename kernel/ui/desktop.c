// kernel/ui/desktop.c dosyasının başı
#include <stdint.h>
#include <ui/desktop.h>
#include <ui/wm.h>             // include/ui/wm.h konumundaysa
#include <ui/mouse.h>
#include <app/app.h>           // include/app/app.h konumundaysa
#include <app/app_manager.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/input.h>
#include <kernel/drivers/input/keyboard.h>
#include <font/font8x8_basic.h>
#include <ui/theme.h>
#include <arch/x86/io.h>
#include <ui/wallpaper.h>

#define fb_width fb_get_width()
#define fb_height fb_get_height()

#ifndef MOUSE_SPEED
#define MOUSE_SPEED 4
#endif

static int g_wallpaper_enabled = 1; // 0=kapalı (debug için)
static int g_wallpaper_tile    = 0; // 0=centered, 1=tile

volatile uint32_t g_ticks_ms = 0;

// UI Modları için enum
enum ui_mode {
    UI_SHELL = 0,
    UI_DESKTOP = 1
};

enum ui_mode current_ui_mode = UI_SHELL;

static void desktop_draw_wallpaper_centered(void)
{
    if (g_wallpaper_w == 0 || g_wallpaper_h == 0) return;
    
    int sw = (int)fb_get_width();
    int sh = (int)fb_get_height();

    int x0 = (sw - (int)g_wallpaper_w) / 2;
    int y0 = (sh - (int)g_wallpaper_h) / 2;

    for (int y = 0; y < (int)g_wallpaper_h; y++) {
        for (int x = 0; x < (int)g_wallpaper_w; x++) {
            uint32_t c = g_wallpaper_pixels[y * g_wallpaper_w + x];
            fb_putpixel(x0 + x, y0 + y, c);
        }
    }
}

static void draw_text8(int x, int y, uint32_t color, const char* s)
{
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        const uint8_t* glyph = font8x8_basic[c];
        for (int row = 0; row < 8; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (line & (1u << (7 - col))) fb_putpixel(x + col, y + row, color);
            }
        }
        x += 8;
    }
}

static void u32_to_hex8(char out[9], uint32_t v)
{
    static const char* hx = "0123456789ABCDEF";
    out[0] = hx[(v >> 28) & 0xF];
    out[1] = hx[(v >> 24) & 0xF];
    out[2] = hx[(v >> 20) & 0xF];
    out[3] = hx[(v >> 16) & 0xF];
    out[4] = hx[(v >> 12) & 0xF];
    out[5] = hx[(v >> 8)  & 0xF];
    out[6] = hx[(v >> 4)  & 0xF];
    out[7] = hx[(v)       & 0xF];
    out[8] = 0;
}

static void u32_to_dec4(char out[5], uint32_t v) // 0..9999
{
    if (v > 9999) v = 9999;
    out[0] = (char)('0' + (v / 1000) % 10);
    out[1] = (char)('0' + (v / 100)  % 10);
    out[2] = (char)('0' + (v / 10)   % 10);
    out[3] = (char)('0' + (v)        % 10);
    out[4] = 0;
}

static void desktop_draw_wallpaper_tile(void)
{
    if (g_wallpaper_w == 0 || g_wallpaper_h == 0) return;

    int sw = (int)fb_get_width();
    int sh = (int)fb_get_height();

    for (int y = 0; y < sh; y++) {
        int wy = y % (int)g_wallpaper_h;
        for (int x = 0; x < sw; x++) {
            int wx = x % (int)g_wallpaper_w;
            uint32_t c = g_wallpaper_pixels[wy * g_wallpaper_w + wx];
            fb_putpixel(x, y, c);
        }
    }
}

static void desktop_draw_background(void)
{
    const ui_theme_t* th = ui_get_theme();

    int w = (int)fb_get_width();
    int h = (int)fb_get_height();

    // 1) Theme bg
    fb_draw_rect(0, 0, w, h, th->desktop_bg);

    // 2) Wallpaper (istersen)
    //if (g_wallpaper_enabled) {
    //    if (g_wallpaper_tile) desktop_draw_wallpaper_tile();
    //    else                  desktop_draw_wallpaper_centered();
    //}
}

static void ui_overlay_draw(void)
{
    const ui_theme_t* th = ui_get_theme();

    int sw = (int)fb_get_width();
    int sh = (int)fb_get_height();

    int dock_h = th->dock_height;
    int dock_w = 420; // şimdilik sabit; sonra ikon sayısına göre büyütürüz
    int dock_x = (sw - dock_w) / 2;
    int dock_y = sh - dock_h - th->dock_margin_bottom;

    gfx_fill_round_rect(dock_x, dock_y, dock_w, dock_h,
                        th->dock_radius, th->dock_bg);
}

// kernel/vga.h veya vga.c'de tanımlı olduğunu varsayıyoruz
extern void vga_disable_cursor(void);

void ui_desktop_run(void)
{
    // 1. VGA Donanım İmlecini Gizle
    vga_disable_cursor();

    // 2. Kesmeleri (Interrupts) maskele ve etkinleştir
    // Not: outb(0x21, 0xFF) tüm master PIC kesmelerini kapatır.
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    __asm__ __volatile__("sti");

    // 3. Minimum Giriş İlklendirmesi
    input_init();
    mouse_init(fb_width, fb_height);

    // Pencere yöneticisi ve uygulama yöneticisini şimdilik çalıştırmıyoruz
    // wm_init();
    // appmgr_init();
    // appmgr_start_demo();

    for (;;)
    {
        // Zamanlayıcı artışı (sanal)
        g_ticks_ms += 16;

        // Klavyeyi ve fareyi donanımdan yokla (Polling)
        input_poll();

        // --- FARE HAREKETİ (Sadece imleci hareket ettirmek için) ---
        int dx, dy;
        uint8_t b;
        while (input_mouse_pop(&dx, &dy, &b)) {
            // Hız çarpanı ile fareyi hareket ettir
            mouse_move(dx * MOUSE_SPEED, dy * MOUSE_SPEED);
            g_mouse.buttons = (int)b;
        }

        // --- RENDER (ÇİZİM) KISMI ---
        
        // Önce arka planı çiz (Bu tüm ekranı temizler ve o mavi ekranı oluşturur)
        desktop_draw_background();

        // Varsa ekrandaki sabit yazıları/overlay'leri çiz
        ui_overlay_draw();

        // Yazılımsal Fare İmlecini en üste çiz (Donanım imleci yerine bunu göreceğiz)
        mouse_draw();

        // Tamamlanan kareyi ekrana yansıt (Double Buffering kullanıyorsan)
        fb_present();

        // CPU'yu %100 yormamak için çok kısa bir bekleme eklenebilir
        // for(volatile int i=0; i<10000; i++); 
    }
}

/* --- Linker Hatalarını Gidermek İçin Stub Fonksiyonlar --- */

// 1. Giriş Sistemi Köprüleri
void input_init(void) { kbd_init(); }
void input_poll(void) { /* Şimdilik boş */ }
uint16_t input_kbd_pop_event(void) { return kbd_pop_event(); }
int input_mouse_pop(int* dx, int* dy, uint8_t* b) { 
    *dx = 0; *dy = 0; *b = 0; return 0; 
}

static ui_theme_t internal_theme = { 0xFF202A44, 45, 10, 10, 0xAA000000 };

// 3. Eksik Uygulama Oluşturucular
void demo_app_create(void) { /* Şimdilik boş */ }
void terminal_app_create(void) { /* Şimdilik boş */ }