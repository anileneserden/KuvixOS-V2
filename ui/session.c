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
    printk("[session] switch -> %d\n", (int)s);

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

        printk("[session] running KUI C++ test\n");
        kui_cpp_test_ui_run();
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
        // Şimdilik statik test ekranı.
        // İstersen burada ileride:
        // kui_cpp_test_ui_tick();
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
        // Şimdilik JSON screen için özel scancode handler yok.
        // İleride eklersen buradan çağır:
        // ui_json_screen_handle_scancode(sc);
        (void)sc;
        return;
    }

    if (g_current == UI_SESSION_KUI_TEST) {
        // Şimdilik test session input almıyor.
        (void)sc;
        return;
    }
}