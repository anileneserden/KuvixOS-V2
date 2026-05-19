// ui/session.c
#include <ui/session.h>
#include <lib/shell.h>
#include <kernel/drivers/video/fb_console.h>
#include <ui/ui_init.h>

static ui_session_t g_current = UI_SESSION_NONE;

void ui_session_init(void) {
    ui_init();
    g_current = UI_SESSION_NONE;
}

void ui_session_switch(ui_session_t s) {
    g_current = s;

    // Sadece TTY1 (Shell) oturumunu hayatta bırakıyoruz
    if (s == UI_SESSION_TTY1) {
        fb_console_set_enabled(true);
        fb_console_clear();
        shell_init();      
    }
}

void ui_session_tick(void) {
    if (g_current == UI_SESSION_TTY1) {
        shell_tick();
    }
}

void ui_session_handle_scancode(uint16_t sc) {
    if (g_current == UI_SESSION_TTY1) {
        shell_handle_scancode(sc);
    }
}