#include <kernel/fs/vfs.h>
#include <kernel/fs/fat.h>
#include <lib/commands.h>
#include <lib/string.h>

static void build_path(char* out, int out_sz, const char* arg) {
    out[0] = 0;
    if (!arg || !arg[0]) return;

    if (arg[0] == '/') {
        strncpy(out, arg, out_sz - 1);
        out[out_sz - 1] = 0;
        return;
    }

    const char* cwd = commands_get_cwd();
    strncpy(out, cwd, out_sz - 1);
    out[out_sz - 1] = 0;

    int l = (int)strlen(out);
    if (l > 0 && out[l - 1] != '/') {
        strncat(out, "/", out_sz - 1 - (int)strlen(out));
    }
    strncat(out, arg, out_sz - 1 - (int)strlen(out));
}

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: touch <dosya>\n");
        return;
    }

    char path[256];
    build_path(path, sizeof(path), argv[1]);

    /* sadece FAT root create */
    if (strncmp(path, "/fat/", 5) == 0) {
        const char* fat_sub = path + 5;

        /* şimdilik sadece root create destekli */
        if (strchr(fat_sub, '/')) {
            commands_puts("Hata: Su anda FAT icin sadece root dizinde dosya olusturma destekleniyor.\n");
            return;
        }

        if (fat_create_root_file(fat_sub)) {
            commands_puts("FAT dosyasi olusturuldu: ");
            commands_puts(path);
            commands_puts("\n");
        } else {
            commands_puts("Hata: FAT dosyasi olusturulamadi: ");
            commands_puts(path);
            commands_puts("\n");
        }
        return;
    }

    /* normal VFS */
    int r = vfs_write_all(path, (const uint8_t*)"", 0);
    if (r >= 0) {
        commands_puts("Dosya olusturuldu: ");
        commands_puts(path);
        commands_puts("\n");
    } else {
        commands_puts("Hata: Dosya olusturulamadi: ");
        commands_puts(path);
        commands_puts("\n");
    }
}

REGISTER_COMMAND(touch, cmd_touch, "Bos dosya olusturur");