#include <kernel/fs/fat.h>
#include <kernel/fs/vfs.h>
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

    if (strcmp(out, "/") != 0) {
        int len = (int)strlen(out);
        if (len > 0 && out[len - 1] != '/') {
            strncat(out, "/", out_sz - 1 - (int)strlen(out));
        }
    }

    strncat(out, arg, out_sz - 1 - (int)strlen(out));
}

void cmd_rm(int argc, char** argv) {
    int force_flag = 0;
    int arg_index = 1;

    if (argc >= 2 && strcmp(argv[1], "-f") == 0) {
        force_flag = 1;
        arg_index = 2;
    }

    if (argc <= arg_index) {
        commands_puts("Kullanim: rm [-f] <dosya/dizin>\n");
        return;
    }

    char path[256];
    build_path(path, sizeof(path), argv[arg_index]);

    if (strncmp(path, "/fat/", 5) == 0) {
        const char* fat_sub = path + 5;

        if (fat_delete_file_path(fat_sub)) {
            commands_puts("Silindi: ");
            commands_puts(path);
            commands_puts("\n");
        } else if (!force_flag) {
            commands_puts("Hata: FAT dosyasi silinemedi: ");
            commands_puts(path);
            commands_puts("\n");
        }
        return;
    }

    if (vfs_remove(path)) {
        commands_puts("Silindi: ");
        commands_puts(path);
        commands_puts("\n");
    } else if (!force_flag) {
        commands_puts("Hata: Dosya/dizin silinemedi: ");
        commands_puts(path);
        commands_puts("\n");
    }
}

REGISTER_COMMAND(rm, cmd_rm, "Dosya veya dizini siler");