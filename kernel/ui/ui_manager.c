#include <ui/ui_manager.h>
#include <ui/desktop.h>
#include <ui/debug_3d_render.h>
#include <kernel/drivers/input/keyboard.h>
#include <stdbool.h>

// Değişkeni burada TANIMLAMA, sadece dışarıdan (kmain'den) geleceğini belirt.
extern int g_current_mode;

void ui_manager_update() {
    // 1. Mod Kontrolü ve Geçiş (Klavye kuyruğunu burada tüketiyoruz)
    if (kbd_has_character()) {
        uint16_t event = kbd_pop_event();
        uint8_t scancode = event & 0x7F;
        bool pressed = !(event & 0x80);

        if (pressed) {
            if (scancode == 0x3B) { // F1
                g_current_mode = MODE_DESKTOP;
                return;
            }
            if (scancode == 0x3C) { // F2
                g_current_mode = MODE_3D_RENDER;
                return;
            }
        }
    }

    // 2. Aktif Modu Çalıştır
    if (g_current_mode == MODE_DESKTOP) {
        ui_desktop_run(); 
    } else {
        debug_3d_render_loop(800, 600);
    }
}