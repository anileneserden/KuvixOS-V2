#include <ui/session.h>
#include <lib/string.h>
#include <kernel/power.h>

void tui_execute_action(const char* a)
{
    if (!a) return;

    if (!strncmp(a, "session:", 8)) {

        const char* s = a + 8;

        if (!strcmp(s, "tty1"))
            ui_session_switch(UI_SESSION_TTY1);

        if (!strcmp(s, "desktop"))
            ui_session_switch(UI_SESSION_DESKTOP);

        return;
    }

    if (!strncmp(a, "sys:", 4)) {

        const char* s = a + 4;

        if (!strcmp(s, "reboot"))
            power_reboot();

        if (!strcmp(s, "poweroff"))
            power_shutdown();

        return;
    }
}