#include <lib/commands.h>
#include <kernel/exec/task.h>

static void cmd_task(int argc, char** argv) {
    commands_puts("--- Arka Plan Gorevleri ---\n");
    int count = 0;

    for (int i = 0; i < MAX_BACKGROUND_TASKS; i++) {
        if (g_bg_tasks[i].active) {
            commands_printf("PID: %u | Yol: %s\n", g_bg_tasks[i].id, g_bg_tasks[i].path);
            count++;
        }
    }

    if (count == 0) {
        commands_puts("Arka planda calisan aktif surec yok.\n");
    }
}

REGISTER_COMMAND(task, cmd_task, "task - Arka plandaki gorevleri listeler");