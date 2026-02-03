#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <ui/theme.h>
#include <arch/x86/io.h>
#include <stdbool.h>

extern int mouse_x;
extern int mouse_y;

// desktop.c
static char debug_kbd_buffer[64] = "Klavye Bekleniyor...";
static int debug_kbd_ptr = 0;

// HATA 3 ÇÖZÜMÜ: Fonksiyonu burada (yukarıda) tanımla veya deklare et
static void local_int_to_str(int num, char* str) {
    int i = 0;
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    if (num < 0) { num = -num; } // Basitleştirilmiş
    while (num != 0) { str[i++] = (num % 10) + '0'; num = num / 10; }
    str[i] = '\0';
    // Ters çevirme... (Şimdilik debug amaçlı ters de kalsa olur veya manuel çevir)
}

void ui_desktop_run(void) {
    ui_theme_bootstrap_default();
    appmgr_init();
    wm_init();
    appmgr_start_app(2);

    const int MAX_X = 1024; 
    const int MAX_Y = 768;
    char x_str[16], y_str[16];
    uint8_t prev_btn = 0; // Tıklama takibi için

    while(1) {
        ps2_mouse_poll(); 
        int dx = 0, dy = 0;
        uint8_t btn = 0;

        while (ps2_mouse_pop(&dx, &dy, &btn)) {
            mouse_x += dx;
            mouse_y += dy;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > MAX_X - 5) mouse_x = MAX_X - 5;
            if (mouse_y > MAX_Y - 5) mouse_y = MAX_Y - 5;

            // HATA 1 ÇÖZÜMÜ: 5 parametre gönderiyoruz
            uint8_t pressed = (btn && !prev_btn);
            uint8_t released = (!btn && prev_btn);
            wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);
            prev_btn = btn;
        }

        fb_clear(0x182838);
        
        // HATA 2 ÇÖZÜMÜ: wm_draw_all yerine wm_draw (Hata mesajındaki öneri)
        wm_draw(); 

        local_int_to_str(mouse_x, x_str);
        local_int_to_str(mouse_y, y_str);
        
        gfx_draw_text(10, 10, 0xFFFFFF, "KuvixOS Desktop");

        // Ekranın sol üst köşesine (veya belirgin bir yere) klavye test yazısını bas
        gfx_draw_text(10, 30, 0xFFFF00, "KLAVYE TEST:"); // Sarı renk
        gfx_draw_text(120, 10, 0x00FF00, debug_kbd_buffer); // Yeşil renk
        
        // Fare İmleci (HATA: gfx_draw_rect silindi, sadece fill kaldı)
        gfx_fill_rect(mouse_x, mouse_y, 6, 6, 0xFFFFFF); 

        fb_present();
        for(volatile int i = 0; i < 500; i++); 
    }
}