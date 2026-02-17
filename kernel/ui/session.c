#include <ui/session.h>
#include <lib/shell.h>
#include <ui/desktop.h>
#include <kernel/drivers/video/fb_console.h>

static ui_session_t g_current = UI_SESSION_NONE;

void ui_session_init(void) {
    g_current = UI_SESSION_NONE;
}

void ui_session_switch(ui_session_t s) {

    g_current = s;

    if (s == UI_SESSION_TTY1) {
        fb_console_clear();
        shell_init();   // blocking ama test için yeterli
    }
    else if (s == UI_SESSION_DESKTOP) {
        ui_desktop_run(); // blocking
    }
}

void ui_session_tick(void) {
    // blocking olduğu için boş
}

void ui_session_handle_scancode(uint16_t sc) {
    (void)sc;
}