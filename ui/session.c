#include <ui/session.h>
#include <lib/shell.h>
#include <ui/desktop.h>
#include <kernel/drivers/video/fb_console.h>
#include <ui/icons.h>
#include <ui/ui_init.h>
#include <kernel/printk.h>

static ui_session_t g_current = UI_SESSION_NONE;

void ui_session_init(void) {
    ui_init();
    g_current = UI_SESSION_NONE;
}

void ui_session_switch(ui_session_t s) {
    g_current = s;

    if (s == UI_SESSION_TTY1) {
        fb_console_set_enabled(true);
        fb_console_clear();
        shell_init();      // ✅ init only
    } else if (s == UI_SESSION_DESKTOP) {
        fb_console_set_enabled(false);
        ui_desktop_init(); // ✅ init only (bunu ekleyeceğiz)
    }
}

void ui_session_tick(void) {
    if (g_current == UI_SESSION_TTY1) {
        shell_tick();
    } else if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_tick(); // ✅ desktop loop buraya taşınacak
    }
}

void ui_session_handle_scancode(uint16_t sc) {
    if (g_current == UI_SESSION_TTY1) {
        shell_handle_scancode(sc);
    } else if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_handle_scancode(sc); // desktop.c içindeki klavye kısmını fonksiyona ayır
    }
}