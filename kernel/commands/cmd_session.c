#include <ui/session_runtime.h>

#include <lib/commands.h>
#include <lib/string.h>

static void cmd_session(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim:\n");
        commands_puts("  session run <desktop.json>\n");
        commands_puts("  session kill\n");
        commands_puts("  session status\n");
        commands_puts("  session autorun\n");
        return;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3 || !argv[2] || !argv[2][0]) {
            commands_puts("Kullanim: session run <desktop.json>\n");
            return;
        }

        if (session_run_path(argv[2])) {
            commands_puts("session: shell baslatildi\n");
        } else {
            commands_puts("session: shell baslatilamadi\n");
        }
        return;
    }

    if (strcmp(argv[1], "kill") == 0) {
        if (session_kill_active()) {
            commands_puts("session: shell kapatildi\n");
        } else {
            commands_puts("session: aktif shell yok\n");
        }
        return;
    }

    if (strcmp(argv[1], "status") == 0) {
        if (session_is_shell_running()) {
            commands_puts("session: aktif shell = ");
            commands_puts(session_active_path());
            commands_puts("\n");
        } else {
            commands_puts("session: aktif shell yok\n");
        }
        return;
    }

    if (strcmp(argv[1], "autorun") == 0) {
        if (session_autorun_from_config()) {
            commands_puts("session: autorun basarili\n");
        } else {
            commands_puts("session: autorun basarisiz\n");
        }
        return;
    }

    commands_puts("session: bilinmeyen alt komut\n");
}

REGISTER_COMMAND(session, cmd_session, "Shell session yonetimi");