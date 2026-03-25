#include <ui/session.h>
#include <lib/shell.h>
#include <ui/desktop.h>
#include <ui/theme.h>
#include <ui/json_screen.h>
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
    printk("[session] switch -> %d\n", (int)s);

    if (s == UI_SESSION_TTY1) {
        fb_console_set_enabled(true);
        fb_console_clear();
        shell_init();
    } else if (s == UI_SESSION_DESKTOP) {
        fb_console_set_enabled(false);
        ui_desktop_init();
    } else if (s == UI_SESSION_JSON_SCREEN) {
        fb_console_set_enabled(false);
        ui_json_screen_init();
    }
}

void ui_session_tick(void) {
    if (g_current == UI_SESSION_TTY1) {
        shell_tick();
    } else if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_tick();
    } else if (g_current == UI_SESSION_JSON_SCREEN) {
        ui_json_screen_tick();
    }
}

void ui_session_handle_scancode(uint16_t sc) {
    if (g_current == UI_SESSION_TTY1) {
        shell_handle_scancode(sc);
    } else if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_handle_scancode(sc);
    } else if (g_current == UI_SESSION_JSON_SCREEN) {
        ui_json_screen_tick();
    }
}