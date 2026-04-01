#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/kvxfs.h>

static int ls_cb(const char* path, uint32_t size, void* u) {
    vfs_stat_t st;
    if (vfs_stat(path, &st) == 0) {
        if (st.type == VFS_T_DIR) {
            fb_console_set_color(0x000000FF, 0x00000000); // mavi (dizin)
        } else {
            fb_console_set_color(0x00FFFFFF, 0x00000000); // beyaz (dosya)
        }
    }

    commands_puts(path);
    commands_puts("\n");

    // Varsayılan renge dön
    fb_console_set_color(0x00FFFFFF, 0x00000000);
    return 0;
}

void cmd_ls(int argc, char** argv) {
    const char* input = (argc > 1) ? argv[1] : "";
    char resolved[VFS_PATH_MAX];

    if (!input[0]) {
        strncpy(resolved, vfs_get_cwd(), sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = 0;
    } else {
        if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
            commands_puts("Hata: yol cozumlenemedi.\n");
            return;
        }
    }

    if (strncmp(resolved, "/persist", 8) == 0) {
        // Burada kvxfs_list_all kendi çıktısını üretiyor.
        // Eğer renkli yapmak istiyorsan kvxfs_list_all içine benzer mantık eklemen lazım.
        kvxfs_list_all(resolved);
    } else {
        vfs_list(resolved, ls_cb, NULL);
    }
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");
