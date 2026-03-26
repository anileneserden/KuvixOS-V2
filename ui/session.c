#include <ui/session.h>

#include <lib/shell.h>
#include <ui/desktop.h>
#include <ui/json_screen.h>
#include <ui/ui_init.h>

#include <ui/theme.h>
#include <ui/icons.h>

#include <ui/kui/cpp/test_ui_c_bridge.h>

#include <kernel/drivers/video/fb_console.h>
#include <kernel/printk.h>

static ui_session_t g_current = UI_SESSION_NONE;

ui_session_t ui_session_current(void) {
    return g_current;
}

void ui_session_init(void) {
    ui_init();
    g_current = UI_SESSION_NONE;
}

void ui_session_switch(ui_session_t s) {
    g_current = s;

    if (s == UI_SESSION_TTY1) {
        fb_console_set_enabled(true);
        fb_console_clear();
        shell_init();
        return;
    }

    if (s == UI_SESSION_DESKTOP) {
        fb_console_set_enabled(false);
        ui_desktop_init();
        return;
    }

    if (s == UI_SESSION_JSON_SCREEN) {
        fb_console_set_enabled(false);
        ui_json_screen_init();
        return;
    }

    if (s == UI_SESSION_KUI_TEST) {
        fb_console_set_enabled(false);
        fb_console_clear();
        kui_cpp_test_ui_init();
        return;
    }
}

void ui_session_tick(void) {
    if (g_current == UI_SESSION_TTY1) {
        shell_tick();
        return;
    }

    if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_tick();
        return;
    }

    if (g_current == UI_SESSION_JSON_SCREEN) {
        ui_json_screen_tick();
        return;
    }

    if (g_current == UI_SESSION_KUI_TEST) {
        kui_cpp_test_ui_tick();
        return;
    }
}

void ui_session_handle_scancode(uint16_t sc) {
    if (g_current == UI_SESSION_TTY1) {
        shell_handle_scancode(sc);
        return;
    }

    if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_handle_scancode(sc);
        return;
    }

    if (g_current == UI_SESSION_JSON_SCREEN) {
        (void)sc;
        return;
    }

    if (g_current == UI_SESSION_KUI_TEST) {
        (void)sc;
        return;
    }
}