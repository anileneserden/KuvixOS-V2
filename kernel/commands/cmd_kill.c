#include <lib/commands.h>
#include <kernel/exec/task.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

static void cmd_kill(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("kullanim: kill <pid>\n");
        return;
    }

    // Basit bir string'den sayıya çevirme (atoi benzeri)
    uint32_t target_id = 0;
    const char* p = argv[1];
    while (*p >= '0' && *p <= '9') {
        target_id = target_id * 10 + (*p - '0');
        p++;
    }

    int found = 0;
    for (int i = 0; i < MAX_BACKGROUND_TASKS; i++) {
        if (g_bg_tasks[i].active && g_bg_tasks[i].id == target_id) {
            // Belleği güvenle temizle
            if (g_bg_tasks[i].buffer) {
                kfree(g_bg_tasks[i].buffer);
            }
            g_bg_tasks[i].active = 0;
            g_bg_tasks[i].id = 0;
            g_bg_tasks[i].buffer = 0;
            
            commands_printf("[i] PID %u sonlandirildi ve bellek temizlendi.\n", target_id);
            found = 1;
            break;
        }
    }

    if (!found) {
        commands_printf("kill: PID %u bulunamadi!\n", target_id);
    }
}

REGISTER_COMMAND(kill, cmd_kill, "kill <pid> - Arka plandaki bir gorevi sonlandirir");