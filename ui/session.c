#include <ui/session.h>
#include <lib/shell.h>
#include <ui/desktop.h>
#include <kernel/drivers/video/fb_console.h>
#include <ui/icons.h>
#include <ui/ui_init.h>
#include <ui/inputtest.h>
#include <ui/theme.h>
#include <ui/tui/tui_cfg.h>
#include <ui/tui/tui.h>

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
        ui_theme_bootstrap_default();
        ui_desktop_init(); // ✅ init only (bunu ekleyeceğiz)
    } else if (s == UI_SESSION_INPUT) {
        fb_console_set_enabled(false);
        inputtest_init(); // ✅ init only (bunu ekleyeceğiz)
    } else if (s == UI_SESSION_TUI) {
        fb_console_set_enabled(false);
        fb_console_clear();
        tui_clear();
        tui_load_cfg("/system/tui/main.cfg");
        tui_init();
    }
}

void ui_session_tick(void) {
    if (g_current == UI_SESSION_TTY1) {
        shell_tick();
    } else if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_tick();
    } else if (g_current == UI_SESSION_INPUT) {
        inputtest_tick();
    } else if (g_current == UI_SESSION_TUI) {
        tui_tick();
    }
}

void ui_session_handle_scancode(uint16_t sc) {
    if (g_current == UI_SESSION_TTY1) {
        shell_handle_scancode(sc);
    } else if (g_current == UI_SESSION_DESKTOP) {
        ui_desktop_handle_scancode(sc);
    } else if (g_current == UI_SESSION_INPUT) {
        inputtest_handle_scancode(sc);
    } else if (g_current == UI_SESSION_TUI) {
        tui_handle_scancode(sc);
    }
}