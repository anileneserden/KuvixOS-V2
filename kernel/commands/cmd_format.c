#include <kernel/printk.h>
#include <kernel/fs/kvxfs.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_format(int argc, char** argv) {
    int force = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--force") == 0) force = 1;
    }
    if (!force) {
        commands_puts("Kullanim: format --force\n");
        commands_puts("UYARI: Bu islem KVXFS verilerini siler.\n");
        return;
    }

    commands_puts("KVXFS format baslatiliyor...\n");
    if (kvxfs_force_format()) {
        commands_puts("OK: KVXFS formatlandi.\n");
    } else {
        commands_puts("HATA: KVXFS format basarisiz.\n");
    }
}

REGISTER_COMMAND(format, cmd_format, "Kalıcı diski (KVXFS) manuel olarak formatlar");