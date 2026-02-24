#include <kernel/printk.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <lib/commands.h>
#include <stdint.h>
#include <stdbool.h>

static void rm_usage(void) {
    commands_puts("Kullanim: rm [-f] <path>\n");
    commands_puts("  -f   hata mesajlarini bastirir\n");
    commands_puts("Not: -r/-R (klasor silme) henuz yok.\n");
}

// ------------------------------------------------------------
// rm command
// ------------------------------------------------------------
void cmd_rm(int argc, char** argv) {
    bool force = false;
    const char* path = 0;

    // argv[0] = "rm"
    if (argc < 2) {
        rm_usage();
        return;
    }

    // arg parse
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!a) continue;

        if (a[0] == '-') {
            // flags
            if (streq(a, "-f")) { force = true; continue; }
            if (streq(a, "-r") || streq(a, "-R")) {
                if (!force) commands_puts("rm: -r/-R henuz desteklenmiyor.\n");
                return;
            }
            if (!force) {
                commands_puts("rm: bilinmeyen parametre: ");
                commands_puts(a);
                commands_puts("\n");
                rm_usage();
            }
            return;
        } else {
            // ilk path’i al
            if (!path) path = a;
            else {
                // birden fazla path gelirse şimdilik hata
                if (!force) {
                    commands_puts("rm: su an sadece tek path destekleniyor.\n");
                }
                return;
            }
        }
    }

    if (!path || !path[0]) {
        rm_usage();
        return;
    }

    // (opsiyonel) root silmeyi engelle
    if (streq(path, "/")) {
        if (!force) commands_puts("rm: '/' silinemez.\n");
        return;
    }

    // 1) remove dene
    int r = vfs_remove(path);
    if (r >= 0) {
        commands_puts("rm: silindi: ");
        commands_puts(path);
        commands_puts("\n");
        return;
    }

    // 2) fallback: remove yoksa/olmuyorsa truncate dene
    int w = vfs_write_all(path, (const uint8_t*)"", 0);
    if (w >= 0) {
        if (!force) {
            commands_puts("rm: unlink basarisiz, dosya 0 byte yapildi: ");
            commands_puts(path);
            commands_puts("\n");
        }
        return;
    }

    if (!force) {
        commands_puts("rm: silinemedi: ");
        commands_puts(path);
        commands_puts("\n");
    }
}

REGISTER_COMMAND(rm, cmd_rm, "Dosya siler (rm [-f] <path>)");