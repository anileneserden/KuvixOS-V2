// ui/tui/tui_action.c
#include <ui/tui/tui_action.h>
#include <ui/tui/tui_cfg.h>
#include <ui/tui/tui.h>

#include <ui/session.h>

#include <kernel/power.h>          // poweroff/reboot fonksiyonların neredeyse
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

static int starts_with(const char* s, const char* p) {
    if (!s || !p) return 0;
    while (*p) {
        if (*s++ != *p++) return 0;
    }
    return 1;
}

static const char* skip_ws(const char* s) {
    while (s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    return s;
}

void tui_execute_action(const char* action) {
    if (!action) return;
    action = skip_ws(action);
    if (!action[0]) return;

    // ----------------------------
    // cmd: -> commands_execute()
    // ör: cmd:layout trq
    // ----------------------------
    if (starts_with(action, "cmd:")) {
        const char* cmd = skip_ws(action + 4);
        if (cmd && cmd[0]) {
            commands_execute(cmd);
        }
        return;
    }

    // ----------------------------
    // cfg: -> başka menü yükle
    // ör: cfg:/system/tui/main.cfg
    // ----------------------------
    if (starts_with(action, "cfg:")) {
        const char* path = skip_ws(action + 4);
        if (path && path[0]) {
            if (tui_load_cfg(path)) {
                tui_init();
            } else {
                printk("[TUI] cfg load failed: %s\n", path);
            }
        }
        return;
    }

    // ----------------------------
    // session:
    // ----------------------------
    if (starts_with(action, "session:")) {
        const char* s = skip_ws(action + 8);

        if (starts_with(s, "tty1"))      ui_session_switch(UI_SESSION_TTY1);
        else if (starts_with(s, "desktop")) ui_session_switch(UI_SESSION_DESKTOP);
        else if (starts_with(s, "tui"))     ui_session_switch(UI_SESSION_TUI);
        else printk("[TUI] unknown session: %s\n", s);

        return;
    }

    // ----------------------------
    // sys:
    // ----------------------------
    if (starts_with(action, "sys:")) {
        const char* s = skip_ws(action + 4);

        if (starts_with(s, "reboot")) {
            power_reboot();
        } else if (starts_with(s, "poweroff")) {
            power_shutdown();
        } else {
            printk("[TUI] unknown sys action: %s\n", s);
        }
        return;
    }

    printk("[TUI] unknown action: %s\n", action);
}