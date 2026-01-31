#include <stdint.h>
#include <ui/desktop.h>
#include <ui/wm.h>
#include <ui/mouse.h>
#include <ui/cursor.h> // Senin eklediğin cursor.h (50x50 bitmap)
#include <app/app.h>
#include <app/app_manager.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/input.h>
#include <kernel/drivers/input/keyboard.h>
#include <font/font8x8_basic.h>
#include <ui/theme.h>
#include <arch/x86/io.h>
#include <ui/wallpaper.h>
#include <kernel/time.h>

// Fare koordinatları için global değişkenler
static int mouse_x = 400;
static int mouse_y = 300;

void demo_app_create(void) { }
void terminal_app_create(void) { }

// Mevcut masaüstü çizim fonksiyonu
static void desktop_draw_background(void) {
    const ui_theme_t* th = ui_get_theme();
    fb_clear(th->desktop_bg); // Temayı kullanarak arka planı boya
}

// Masaüstü altındaki Dock/Bar çizimi
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

void ui_desktop_run(void) {
    // 1. Mouse ve Klavye sistemini başlat
    input_init();
    time_init_from_rtc();
    
    // 2. Ana Döngü
    while(1) {
        // --- Çizim Bölümü ---
        
        // A) Önce arka planı çiz (Hepsini siler)
        desktop_draw_background();

        // B) Masaüstü bileşenlerini çiz (Dock, Pencereler vb.)
        ui_overlay_draw();
        
        // C) Yazıları yaz
        gfx_draw_text(20, 20, 0xFFFFFF, "KuvixOS V2 Desktop");

        // D) IMLECI ÇIZ (Her şeyin en üstünde olmalı)
        // cursor.c içindeki fonksiyonunu burada çağırıyoruz
        cursor_draw_arrow(mouse_x, mouse_y);

        // E) Arka tamponu (backbuffer) gerçek ekrana gönder
        fb_present();

        // --- Giriş/Input Bölümü ---
        
        // Şimdilik mouse verisi gelmediği için mouse_x ve mouse_y sabit kalacak.
        // sti (kesmeler) açıldığında mouse_ps2 sürücünden gelen veriler 
        // buradaki mouse_x ve mouse_y'yi güncelleyecek.
        
        // CPU'yu yormamak için çok kısa bekleme yapılabilir
        // for(volatile int i=0; i<10000; i++); 
    }
}

/* Linker hatalarını önlemek için stub fonksiyonlar */
void input_init(void) { kbd_init(); }
void input_poll(void) { }
uint16_t input_kbd_pop_event(void) { return kbd_pop_event(); }
int input_mouse_pop(int* dx, int* dy, uint8_t* b) { 
    *dx = 0; *dy = 0; *b = 0; return 0; 
}