#include <ui/session.h>
#include <lib/shell.h>
#include <ui/desktop.h>
#include <kernel/drivers/video/fb_console.h>
#include <ui/icons.h>
#include <ui/ui_init.h>
#include <kernel/drivers/video/fb.h>
#include <ui/inputtest.h>

static ui_session_t g_current = UI_SESSION_NONE;

void ui_session_init(void) {
    ui_init();
    g_current = UI_SESSION_NONE;
}

void ui_session_switch(ui_session_t s) {
    g_current = s;

    if (s == UI_SESSION_TTY1) {
        fb_console_set_enabled(true);

        // ✅ önce ekranı gerçekten temizle
        fb_clear(0x00000000);
        fb_present();

        fb_console_init(0x00FFFFFF, 0x00000000); // renkler garanti
        fb_console_clear();                      // cursor reset + clear
        shell_init();                            // prompt’u basacak
    } else if (s == UI_SESSION_DESKTOP) {
        fb_console_set_enabled(false);
        ui_desktop_init();
    } else if (s == UI_SESSION_INPUT) {
        fb_console_set_enabled(false);
        inputtest_init();
    }
}

void ui_session_tick(void) {
    if (g_current == UI_SESSION_TTY1) {
        shell_tick();
    } else if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_tick(); // ✅ desktop loop buraya taşınacak
    } else if (g_current == UI_SESSION_INPUT) {
        inputtest_tick(); // ✅ desktop loop buraya taşınacak
    }
}

void ui_session_handle_scancode(uint16_t sc) {
    if (g_current == UI_SESSION_TTY1) {
        shell_handle_scancode(sc);
    } else if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_handle_scancode(sc); // desktop.c içindeki klavye kısmını fonksiyona ayır
    } else if (g_current == UI_SESSION_INPUT) {
        inputtest_handle_scancode(sc);

        // ESC ile geri dön (Set1 ESC=0x01)
        uint8_t s = (uint8_t)(sc & 0xFF);
        if ((s & 0x80) == 0 && s == 0x01) {
            ui_session_switch(UI_SESSION_TTY1);
        }
    }
}